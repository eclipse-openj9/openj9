FROM registry.access.redhat.com/ubi8/ubi-minimal:latest

LABEL vendor="OpenJ9 Community" \
      name="OpenJ9 Operator" \
      version="0.6.0" \
      release="0.6.0" \
      summary="Image for OpenJ9 Operator" \
      description="This image contains the controller for OpenJ9 Operator."
      
ENV OPERATOR=/usr/local/bin/openj9-operator \
    USER_UID=1001 \
    USER_NAME=openj9-operator

# install operator binary
COPY build/_output/bin/openj9-operator ${OPERATOR}
COPY build/bin /usr/local/bin
COPY LICENSE /licenses/

RUN  /usr/local/bin/user_setup

ENTRYPOINT ["/usr/local/bin/entrypoint"]

USER ${USER_UID}
