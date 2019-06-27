FROM nvidia/opengl:1.0-glvnd-runtime-ubuntu16.04 as nvidia
LABEL description="nvidia driver for OpenGL"
##################################################
FROM osrf/ros:kinetic-desktop-full as builder
LABEL description="ubuntu16.04 with ros-kinetic-desktop-full installed"

COPY --from=nvidia /usr/local /usr/local
COPY --from=nvidia /etc/ld.so.conf.d/glvnd.conf /etc/ld.so.conf.d/glvnd.conf

ENV NVIDIA_VISIBLE_DEVICES=all NVIDIA_DRIVER_CAPABILITIES=all

#build dependencies.
#software-properties-common for add-apt-repository
#ca-certificates for verification
RUN apt-get update && apt-get install -y \
    software-properties-common \
    ca-certificates \
    sudo \
    glmark2 \
    mesa-utils \
    build-essential \
    cmake \
    vim \
    git \
    tar \
    unzip \
    wget \
    curl \
    --fix-missing

#ros plugins.
RUN apt-get install -y \
    ros-kinetic-jsk-recognition-msgs \
    ros-kinetic-nmea-msgs \
    ros-kinetic-grid-map-ros \
    ros-kinetic-jsk-rviz-plugins \
    libglew-dev

#qt5.5
RUN apt install -y  \
    qt5-default \
    qttools5-dev-tools \
    libqt5opengl5-dev

#3rd party
ENV CMAKE_PREFIX_PATH=/usr/local/lib64
ENV LD_LIBRARY_PATH=/usr/local/lib64

WORKDIR /home/zhihui/library

#osg3.4
RUN git clone -b OpenSceneGraph-3.4 https://github.com/openscenegraph/OpenSceneGraph.git && \
    cd OpenSceneGraph && mkdir build && cd build \
    && cmake .. && make -j6 && make install

#osgearth2.8
RUN git clone -b 2.8 https://github.com/gwaldron/osgearth.git && \
    cd osgearth && mkdir build && cd build \
    && cmake -DOSGEARTH_USE_QT=ON .. && make -j6 && make install

#osgQt bug fixes
RUN sed -i 's/<QGLWidget>/<QtOpenGL\/QGLWidget>/g' /usr/local/include/osgQt/GraphicsWindowQt && \
    sed -i 's/<QGLWidget>/<QtOpenGL\/QGLWidget>/g' /usr/local/include/osgQt/QGraphicsViewAdapter
#RUN ln -s /usr/local/lib64/libosgQt5.so /usr/local/lib64/libosgQt.so

#fast-rtps
RUN git clone https://github.com/eProsima/Fast-RTPS && \
    cd Fast-RTPS && mkdir build && cd build \
    && cmake -DTHIRDPARTY=ON .. && make -j6 && make install

####AutoPlanning####
#git clone http://username:password@remote
#username中的@需要转义为%40
WORKDIR /home/zhihui
ENV HOME=/home/zhihui

#visualize branch
RUN git clone -b master https://707060335%40qq.com:199505075@gitee.com/iskyfly_sz/AutoPlanning.git && \
    cd AutoPlanning && mkdir build && cd build && \
    . /opt/ros/kinetic/setup.sh && \
    cmake ../src && make

#start up
ADD startup /home/zhihui
RUN chmod a+x /home/zhihui/startup

CMD ["/home/zhihui/startup"]
##################################################

