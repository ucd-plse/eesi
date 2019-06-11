FROM ubuntu:xenial as dev

LABEL MAINTAINER="dcdefreez@ucdavis.edu"

FROM defreez/eesi:dev as build
COPY . /eesi
WORKDIR /eesi/build
RUN cmake ../src \
	&& make -j$(nproc)

FROM ubuntu:xenial

COPY --from=build /eesi/build /eesi
COPY --from=build /eesi/config /eesi/config
ENV LD_LIBRARY_PATH /eesi
RUN apt-get update -y \
    && apt-get install -y \
	libboost-program-options1.58.0 \
	libsqlite3-0 \
        locales

# Set the locale
RUN sed -i -e 's/# en_US.UTF-8 UTF-8/en_US.UTF-8 UTF-8/' /etc/locale.gen && \
    locale-gen
ENV LANG en_US.UTF-8  
ENV LANGUAGE en_US:en  
ENV LC_ALL en_US.UTF-8 

RUN mkdir /d
WORKDIR /d
ENTRYPOINT ["/eesi/eesi"]
