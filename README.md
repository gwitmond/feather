# feather
A very lightweight web service for Genode platforms.

It's a very simple http web server for the Genode platform. It's
designed to be as simple as possible to make it easy to see what's going on.

As such, it is:
- single threaded;
- static site only;
- just GET and HEAD methods.

It ignores:
- almost all headers;
- query parameters;
- character encoding;
- almost all methods;
- in fact, almost all of the HTTP-protocol.

# Usage

To use, install Genode (http://genode.org) first.

Then:

    mkdir $GENODE_DIR/projects
    cd !$
    git clone github.com/gwitmond/feather

Create a build directory

    $GENODE_DIR/tool/create_build_dir <platform of choice>
    cd !$
    echo 'REPOSITORIES += $(GENODE_DIR)/projects/feather' >> $GENODE_DIR/build/<platform>etc/build.conf

Add a website

    mkdir -p websites/www.example.org
    echo 'Hello Feather' > !$/index.html
    tar cf bin/websites.tar websites

Build and run

    make run/feather
