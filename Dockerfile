FROM ubuntu:16.04

# wget https://raw.githubusercontent.com/CommerciumBlockchain/cmm-qt-wallet/master/Dockerfile -O Dockerfile
# docker build -t cmm .
# docker run -v $(pwd):/host -t cmm

RUN apt-get -y update \
&&  DEBIAN_FRONTEND=noninteractive apt-get -y install \
        g++-mingw-w64-i686 \
# mxe depends
    autoconf \
    automake \
    autopoint \
    bash \
    bison \
    bzip2 \
    flex \
    g++ \
    g++-multilib \
    gettext \
    git \
    gperf \
    intltool \
    libc6-dev-i386 \
    libgdk-pixbuf2.0-dev \
    libltdl-dev \
    libssl-dev \
    libtool-bin \
    libxml-parser-perl \
    make \
    openssl \
    p7zip-full \
    patch \
    perl \
    pkg-config \
    python \
    ruby \
    sed \
    unzip \
    wget \
    xz-utils

WORKDIR /build

RUN git clone https://github.com/mxe/mxe.git
RUN cd mxe && make qtbase

RUN git clone https://github.com/CommerciumBlockchain/cmm-qt-wallet.git

WORKDIR cmm-qt-wallet

#* for windows
RUN export PATH=/build/mxe/usr/bin:$PATH ;\
    /build/mxe/usr/i686-w64-mingw32.static/qt5/bin/qmake cmm-qt-wallet.pro

#* for linux
RUN cp -r /build/cmm-qt-wallet /build/cmm-qt-wallet-linux
WORKDIR /build/cmm-qt-wallet-linux
RUN /build/mxe/usr/x86_64-pc-linux-gnu/qt5/bin/qmake cmm-qt-wallet.pro

ENV CMM Commercium_Continuum
ENV LATESTTAG 1.0.0

RUN printf "#!/bin/bash\n\
mkdir -p /host/linux/$CMM /host/win64/$CMM \n\
git pull \n\
make -j$(nproc) \n\
strip cmm-qt-wallet \n\
cp cmm-qt-wallet /lib/x86_64-linux-gnu/libpng12.so.0 /host/linux/$CMM \n\
cd /host/linux \n\
tar zcf /host/$CMM-$LATESTTAG-qt-linux.tar.gz $CMM \n\
#* back to windows \n\
cd /build/cmm-qt-wallet \n\
export PATH=/build/mxe/usr/bin:$PATH \n\
git pull \n\
make -j$(nproc) \n\
cp release/cmm-qt-wallet.exe /host/win64/$CMM \n\
cd /host/win64 \n\
zip -r /host/$CMM-$LATESTTAG-qt-win64.zip $CMM \n" > run.sh \
&& chmod u+x run.sh

CMD ["/bin/bash", "./run.sh"]
