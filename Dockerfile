#
# Build stage
#
FROM alpine:3.11 AS build
RUN apk add --update --no-cache bash make gcc musl-dev 

COPY * /usr/src/mrhttpd/
COPY html/* /usr/src/mrhttpd/html/
RUN cd /usr/src/mrhttpd && mv mrhttpd-docker.conf mrhttpd.conf && make install


#
# Package stage
#
FROM alpine:3.11
COPY --from=build /usr/local/bin/mrhttpd /usr/local/bin/
COPY --from=build /opt/mrhttpd /opt/mrhttpd/
ENTRYPOINT ["/usr/local/bin/mrhttpd"]
