FROM devkitpro/devkitarm:latest

ENV DEVKITPRO=/opt/devkitpro
ENV DEVKITARM=${DEVKITPRO}/devkitARM
ENV PORTLIBS=${DEVKITPRO}/portlibs/3ds
ENV PATH=${DEVKITARM}/bin:${DEVKITPRO}/tools/bin:${PATH}

RUN dkp-pacman -Syu --noconfirm \
    && dkp-pacman -S --noconfirm 3ds-dev

WORKDIR /workspace
CMD ["/bin/bash"]
