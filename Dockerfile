FROM debian:bookworm-slim AS build

RUN apt-get update \
    && apt-get install -y --no-install-recommends cmake g++ make \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY CMakeLists.txt ./
COPY parser ./parser
COPY runtime ./runtime

RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build --parallel 2

FROM debian:bookworm-slim
COPY --from=build /src/build/pfy /usr/local/bin/pfy

ENTRYPOINT ["pfy"]
CMD ["help"]
