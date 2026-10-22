#include "pfy/binary.hpp"

#include "pfy/error.hpp"

#include <cstring>
#include <limits>
#include <utility>

namespace pfy {
namespace {

constexpr std::uint16_t kVersion = 1;
constexpr std::uint16_t kFlags = 0;
constexpr std::size_t kHeaderSize = 24;
constexpr std::size_t kMaxSource = 16 * 1024 * 1024;
constexpr std::size_t kMaxPayload = 64 * 1024 * 1024;
constexpr std::size_t kMaxString = 16 * 1024 * 1024;
constexpr std::size_t kMaxNodes = 1'000'000;
constexpr int kMaxDepth = 256;

class Writer {
public:
    explicit Writer(std::vector<std::uint8_t>& bytes) : bytes_(bytes) {}

    void u8(std::uint8_t value) { bytes_.push_back(value); }
    void u16(std::uint16_t value) {
        bytes_.push_back(static_cast<std::uint8_t>(value & 0xFF));
        bytes_.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFF));
    }
    void u32(std::uint32_t value) {
        for (int i = 0; i < 4; ++i) bytes_.push_back(static_cast<std::uint8_t>((value >> (i * 8)) & 0xFF));
    }
    void i32(std::int32_t value) { u32(static_cast<std::uint32_t>(value)); }
    void f64(double value) {
        std::uint64_t bits = 0;
        static_assert(sizeof(bits) == sizeof(value), "unexpected double size");
        std::memcpy(&bits, &value, sizeof(bits));
        for (int i = 0; i < 8; ++i) bytes_.push_back(static_cast<std::uint8_t>((bits >> (i * 8)) & 0xFF));
    }
    void text(const std::string& value) {
        u32(static_cast<std::uint32_t>(value.size()));
        bytes_.insert(bytes_.end(), value.begin(), value.end());
    }

private:
    std::vector<std::uint8_t>& bytes_;
};

class Reader {
public:
    Reader(const std::uint8_t* data, std::size_t size) : data_(data), size_(size) {}

    std::size_t remaining() const { return size_ - offset_; }
    std::uint8_t u8() {
        need(1);
        return data_[offset_++];
    }
    std::uint16_t u16() {
        std::uint16_t value = static_cast<std::uint16_t>(u8());
        value |= static_cast<std::uint16_t>(u8()) << 8;
        return value;
    }
    std::uint32_t u32() {
        std::uint32_t value = 0;
        for (int i = 0; i < 4; ++i) value |= static_cast<std::uint32_t>(u8()) << (i * 8);
        return value;
    }
    std::int32_t i32() { return static_cast<std::int32_t>(u32()); }
    double f64() {
        std::uint64_t bits = 0;
        for (int i = 0; i < 8; ++i) bits |= static_cast<std::uint64_t>(u8()) << (i * 8);
        double value = 0.0;
        std::memcpy(&value, &bits, sizeof(value));
        return value;
    }
    std::string text() {
        std::uint32_t length = u32();
        if (length > kMaxString) throw BinaryError("string is too large");
        need(length);
        std::string value(reinterpret_cast<const char*>(data_ + offset_), length);
        offset_ += length;
        return value;
    }

private:
    const std::uint8_t* data_;
    std::size_t size_;
    std::size_t offset_ = 0;

    void need(std::size_t count) const {
        if (count > size_ - offset_) throw BinaryError("unexpected end of binary image");
    }
};

void writeNode(Writer& writer, const NodePtr& node, int depth, std::size_t& count) {
    if (!node) throw BinaryError("cannot encode a null node");
    if (depth > kMaxDepth) throw BinaryError("AST is too deep");
    if (++count > kMaxNodes) throw BinaryError("AST has too many nodes");

    writer.u8(static_cast<std::uint8_t>(node->kind));
    writer.i32(node->line);
    writer.i32(node->column);

    switch (node->kind) {
        case NodeKind::Program:
        case NodeKind::Block:
            writer.u32(static_cast<std::uint32_t>(node->children.size()));
            for (const auto& child : node->children) writeNode(writer, child, depth + 1, count);
            break;
        case NodeKind::Null:
        case NodeKind::Break:
        case NodeKind::Continue:
            break;
        case NodeKind::Bool:
            writer.u8(node->boolean ? 1 : 0);
            break;
        case NodeKind::Number:
            writer.f64(node->number);
            break;
        case NodeKind::String:
        case NodeKind::Identifier:
            writer.text(node->text);
            break;
        case NodeKind::Array:
            writer.u32(static_cast<std::uint32_t>(node->children.size()));
            for (const auto& child : node->children) writeNode(writer, child, depth + 1, count);
            break;
        case NodeKind::Object:
            writer.u32(static_cast<std::uint32_t>(node->fields.size()));
            for (const auto& field : node->fields) {
                writer.text(field.first);
                writeNode(writer, field.second, depth + 1, count);
            }
            break;
        case NodeKind::Unary:
            writer.text(node->text);
            if (node->children.size() != 1) throw BinaryError("malformed unary node");
            writeNode(writer, node->children[0], depth + 1, count);
            break;
        case NodeKind::Binary:
            writer.text(node->text);
            if (node->children.size() != 2) throw BinaryError("malformed binary node");
            writeNode(writer, node->children[0], depth + 1, count);
            writeNode(writer, node->children[1], depth + 1, count);
            break;
        case NodeKind::Call:
            if (node->children.empty()) throw BinaryError("malformed call node");
            writer.u32(static_cast<std::uint32_t>(node->children.size()));
            for (const auto& child : node->children) writeNode(writer, child, depth + 1, count);
            break;
        case NodeKind::Index:
            if (node->children.size() != 2) throw BinaryError("malformed index node");
            writeNode(writer, node->children[0], depth + 1, count);
            writeNode(writer, node->children[1], depth + 1, count);
            break;
        case NodeKind::Member:
            writer.text(node->text);
            if (node->children.size() != 1) throw BinaryError("malformed member node");
            writeNode(writer, node->children[0], depth + 1, count);
            break;
        case NodeKind::Function:
            writer.text(node->text);
            writer.u32(static_cast<std::uint32_t>(node->params.size()));
            for (const auto& parameter : node->params) writer.text(parameter);
            if (node->children.size() != 1) throw BinaryError("malformed function node");
            writeNode(writer, node->children[0], depth + 1, count);
            break;
        case NodeKind::Let:
            writer.text(node->text);
            if (node->children.size() != 1) throw BinaryError("malformed let node");
            writeNode(writer, node->children[0], depth + 1, count);
            break;
        case NodeKind::Set:
            if (node->children.size() != 2) throw BinaryError("malformed set node");
            writeNode(writer, node->children[0], depth + 1, count);
            writeNode(writer, node->children[1], depth + 1, count);
            break;
        case NodeKind::If:
            if (node->children.size() != 3) throw BinaryError("malformed if node");
            for (const auto& child : node->children) writeNode(writer, child, depth + 1, count);
            break;
        case NodeKind::While:
            if (node->children.size() != 2) throw BinaryError("malformed while node");
            for (const auto& child : node->children) writeNode(writer, child, depth + 1, count);
            break;
        case NodeKind::For:
            writer.text(node->text);
            if (node->children.size() != 2) throw BinaryError("malformed for node");
            for (const auto& child : node->children) writeNode(writer, child, depth + 1, count);
            break;
        case NodeKind::Return:
            if (node->children.size() != 1) throw BinaryError("malformed return node");
            writeNode(writer, node->children[0], depth + 1, count);
            break;
        case NodeKind::Expression:
            if (node->children.size() != 1) throw BinaryError("malformed expression node");
            writeNode(writer, node->children[0], depth + 1, count);
            break;
    }
}

NodePtr readNode(Reader& reader, int depth, std::size_t& count) {
    if (depth > kMaxDepth) throw BinaryError("AST is too deep");
    if (++count > kMaxNodes) throw BinaryError("AST has too many nodes");

    auto node = Node::make(NodeKind::Null);
    std::uint8_t tag = reader.u8();
    if (tag < static_cast<std::uint8_t>(NodeKind::Program) ||
        tag > static_cast<std::uint8_t>(NodeKind::Expression)) {
        throw BinaryError("unknown node tag");
    }
    node->kind = static_cast<NodeKind>(tag);
    node->line = reader.i32();
    node->column = reader.i32();

    auto children = [&](std::size_t countValue) {
        node->children.clear();
        for (std::size_t i = 0; i < countValue; ++i) {
            node->children.push_back(readNode(reader, depth + 1, count));
        }
    };

    switch (node->kind) {
        case NodeKind::Program:
        case NodeKind::Block: {
            std::uint32_t n = reader.u32();
            if (n > kMaxNodes) throw BinaryError("invalid child count");
            children(n);
            break;
        }
        case NodeKind::Null:
        case NodeKind::Break:
        case NodeKind::Continue:
            break;
        case NodeKind::Bool:
            node->boolean = reader.u8() != 0;
            break;
        case NodeKind::Number:
            node->number = reader.f64();
            break;
        case NodeKind::String:
        case NodeKind::Identifier:
            node->text = reader.text();
            break;
        case NodeKind::Array: {
            std::uint32_t n = reader.u32();
            if (n > kMaxNodes) throw BinaryError("invalid array size");
            children(n);
            break;
        }
        case NodeKind::Object: {
            std::uint32_t n = reader.u32();
            if (n > kMaxNodes) throw BinaryError("invalid field count");
            node->fields.clear();
            for (std::uint32_t i = 0; i < n; ++i) {
                std::string key = reader.text();
                node->fields.emplace_back(std::move(key), readNode(reader, depth + 1, count));
            }
            break;
        }
        case NodeKind::Unary:
            node->text = reader.text();
            children(1);
            break;
        case NodeKind::Binary:
            node->text = reader.text();
            children(2);
            break;
        case NodeKind::Call: {
            std::uint32_t n = reader.u32();
            if (n == 0 || n > kMaxNodes) throw BinaryError("invalid call arity");
            children(n);
            break;
        }
        case NodeKind::Index:
            children(2);
            break;
        case NodeKind::Member:
            node->text = reader.text();
            children(1);
            break;
        case NodeKind::Function: {
            node->text = reader.text();
            std::uint32_t n = reader.u32();
            if (n > 255) throw BinaryError("too many parameters");
            node->params.clear();
            for (std::uint32_t i = 0; i < n; ++i) node->params.push_back(reader.text());
            children(1);
            break;
        }
        case NodeKind::Let:
            node->text = reader.text();
            children(1);
            break;
        case NodeKind::Set:
            children(2);
            break;
        case NodeKind::If:
            children(3);
            break;
        case NodeKind::While:
            children(2);
            break;
        case NodeKind::For:
            node->text = reader.text();
            children(2);
            break;
        case NodeKind::Return:
            children(1);
            break;
        case NodeKind::Expression:
            children(1);
            break;
    }
    return node;
}

} // namespace

std::uint32_t crc32(const std::uint8_t* data, std::size_t size) {
    std::uint32_t crc = 0xFFFFFFFFu;
    for (std::size_t i = 0; i < size; ++i) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
        }
    }
    return ~crc;
}

std::vector<std::uint8_t> encode(const NodePtr& program, const std::string& source) {
    if (source.size() > kMaxSource) throw BinaryError("source is too large");
    std::vector<std::uint8_t> payload;
    Writer payloadWriter(payload);
    std::size_t count = 0;
    writeNode(payloadWriter, program, 0, count);
    if (payload.size() > kMaxPayload) throw BinaryError("compiled payload is too large");

    std::vector<std::uint8_t> image;
    image.reserve(kHeaderSize + source.size() + payload.size());
    Writer writer(image);
    writer.u8('P');
    writer.u8('F');
    writer.u8('Y');
    writer.u8('B');
    writer.u16(kVersion);
    writer.u16(kFlags);
    writer.u32(static_cast<std::uint32_t>(source.size()));
    writer.u32(crc32(reinterpret_cast<const std::uint8_t*>(source.data()), source.size()));
    writer.u32(static_cast<std::uint32_t>(payload.size()));
    writer.u32(crc32(payload.data(), payload.size()));
    image.insert(image.end(), source.begin(), source.end());
    image.insert(image.end(), payload.begin(), payload.end());
    return image;
}

BinaryImage decode(const std::vector<std::uint8_t>& bytes) {
    if (bytes.size() < kHeaderSize) throw BinaryError("file is too short to be a .pfyb image");
    if (!(bytes[0] == 'P' && bytes[1] == 'F' && bytes[2] == 'Y' && bytes[3] == 'B')) {
        throw BinaryError("missing PFYB signature");
    }
    Reader header(bytes.data() + 4, bytes.size() - 4);
    std::uint16_t version = header.u16();
    std::uint16_t flags = header.u16();
    if (version != kVersion) throw BinaryError("unsupported .pfyb version");
    if (flags != 0) throw BinaryError("unsupported .pfyb flags");
    std::uint32_t sourceSize = header.u32();
    std::uint32_t sourceCrc = header.u32();
    std::uint32_t payloadSize = header.u32();
    std::uint32_t payloadCrc = header.u32();
    if (sourceSize > kMaxSource || payloadSize > kMaxPayload) throw BinaryError("binary image is too large");
    if (bytes.size() != kHeaderSize + static_cast<std::size_t>(sourceSize) + payloadSize) {
        throw BinaryError("binary image has an invalid length");
    }

    const std::uint8_t* sourceData = bytes.data() + kHeaderSize;
    const std::uint8_t* payloadData = sourceData + sourceSize;
    if (crc32(sourceData, sourceSize) != sourceCrc) throw BinaryError("source checksum mismatch");
    if (crc32(payloadData, payloadSize) != payloadCrc) throw BinaryError("payload checksum mismatch");

    BinaryImage image;
    image.source.assign(reinterpret_cast<const char*>(sourceData), sourceSize);
    Reader payload(payloadData, payloadSize);
    std::size_t count = 0;
    image.program = readNode(payload, 0, count);
    if (payload.remaining() != 0) throw BinaryError("trailing bytes after the AST");
    if (image.program->kind != NodeKind::Program) throw BinaryError("image root is not a program");
    return image;
}

} // namespace pfy
