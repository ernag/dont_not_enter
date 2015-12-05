#!/usr/bin/env python

__author__ = 'justin'

import SocketServer
import logging
import threading
import asyncore
import socket
import sys
import time

class Handler(SocketServer.StreamRequestHandler):

    def __str__(self):
        return "{0}:{1}".format(self.client_address[0], self.client_address[1])

    def handle(self):
        logging.debug("%s handling new connection.", str(self))
        for data in self.rfile:
            self.receive(data)
        logging.info("%s will finish.", str(self))


    def send(self, data):
        self.wfile.write(data)

    def close(self):
        # self.connection is a socket.
        self.connection.shutdown(socket.SHUT_RDWR)
        self.connection.close()

    def receive(self, data):

        """
        Pass the data off to our server (Controller or Door)
        for handling.
        """

        self.server.receive(data)

class ControllerHandler(Handler):

    def handle(self):
        self.server.add_handler(self)
        Handler.handle(self)

    def finish(self):
        logging.info("ControllerHandler cleaning up.")
        self.server.remove_handler(self)

class ControllerBridge(SocketServer.ThreadingMixIn, SocketServer.TCPServer):

    """
    ControllerBridge is an asynchronous server owned by a DoorServer. It allows N
    controllers to connect; each controller can send events to the door, and events
    from the door will be broadcast to all controllers.
    """

    allow_reuse_address =  True

    def __init__(self, doorserver = None, *args, **kwargs):
        SocketServer.TCPServer.__init__(self, *args, **kwargs)
        self._doorserver = doorserver
        self._handlers = set()
        self._bridge_thread = None
        self._exc_info = None
        logging.info("%s bound.", str(self))

    def __str__(self):
        return "Controller Bridge on {0} {1}".format(*self.server_address)

    def exc_info(self):
        return self._exc_info

    def add_handler(self, handler):
        self._handlers.add(handler)

    def remove_handler(self, handler):
        self._handlers.remove(handler)

    def start(self):

        """
        Start the controller bridge, in its own thread.  Returns immediately.
        """

        if self._bridge_thread is None:
            self._bridge_thread = threading.Thread(target=self.serve_forever)
            self._bridge_thread.daemon = True
            self._bridge_thread.start()
            logging.debug("%s running in thread: %s", str(self), self._bridge_thread.getName())

    def shutdown(self):

        """
        Shut down the controller bridge, and wait until shutdown has completed.
        """

        SocketServer.TCPServer.shutdown(self)
        for handler in self._handlers:
            handler.close()
        self._bridge_thread.join()
        logging.debug("%s shutdown complete.", str(self))

    def receive(self, data):

        """
        Receive data from a single controller, and forward it to the door.
        """

        if self._doorserver is not None:
            logging.info("SEND controller -> door: %s", data)
            self._doorserver.send(data)

    def send(self, data):

        """
        Send data to all controllers connected to the bridge.
        """

        for handler in self._handlers:
            logging.info("SEND door -> controller(%s): %s", str(handler), data)
            handler.send(data)

class DoorHandler(Handler):

    """
    Handles traffic from a connected door. There is only one of these at any time.
    """

    def __str__(self):
        return "DoorHandler {0}".format(Handler.__str__(self))

    def handle(self):
        self.server.set_current_handler(self)
        Handler.handle(self)

    def finish(self):
        self.server.set_current_handler(None)
        logging.info("%s finished.", str(self))


class DoorServer(SocketServer.TCPServer):

    """
    The DoorServer is a synchronous server (only handles one client at a time).

    Upon connection, it creates a ControllerBridge, which is an async server.
    The ControllerBridge allows N controllers to connect to this door. Anything sent by
    one of the controllers is routed to the door, and anything sent by the door is
    routed to all of the controllers.
    """

    allow_reuse_address = True

    def __init__(self, *args, **kwargs):
        SocketServer.TCPServer.__init__(self, *args, **kwargs)
        self._bridge = None
        self._bridge_thread = None
        self._current_handler = None
        self._exc_info = None

    def exc_info(self):
        return self._exc_info

    def send(self, data):

        """
        Send data to the current door, if any (otherwise dropped).
        """

        if self._current_handler is not None:
            self._current_handler.wfile.write(data)

    def receive(self, data):

        """
        Receive data from the current door and forward it to the bridge.
        """
        if self._bridge is not None:
            self._bridge.send(data)

    def set_current_handler(self, handler):

        self._current_handler = handler

    def start_bridge(self):

        """
        Start the controller bridge.  Returns immediately.
        """

        if self._bridge is None:
            self._bridge = ControllerBridge(
                doorserver,
                DoorServer.make_bridge_address(self.server_address),
                ControllerHandler,
            )
            self._bridge.start()

    def start(self):
        self._server_thread = threading.Thread(target=self.serve_forever)
        self._server_thread.daemon = True
        self._server_thread.start()
        self.start_bridge()
        logging.info("Doorserver running in thread: %s", self._server_thread.name)

    def stop_bridge(self):

        """
        Stop the bridge, if any.
        """

        if self._bridge is not None:
            logging.debug("Stopping bridge.")
            self._bridge.shutdown()
            self._bridge = None
            logging.debug("Bridge stopped.")

    def shutdown(self):

        """
        Shut down the doorserver, controller bridge, and any open handlers, and wait until
        shutdown is complete before returning.
        """

        logging.debug("Shutting down doorserver.")

        self.stop_bridge()

        if self._current_handler is not None:
            logging.debug("Closing door handler.")
            self._current_handler.close()
            logging.debug("Door handler closed.")

        SocketServer.TCPServer.shutdown(self)
        self._server_thread.join()
        logging.debug("Doorserver shutdown complete.")

    @staticmethod
    def make_bridge_address(addrinfo):
        return (addrinfo[0], addrinfo[1] + 1)

class BridgeClose(IOError): pass

class ClientBridge(object):

    """
    Simple proxy object; bridges two sockets.
    """

    class Client(asyncore.dispatcher):

        """
        One endpoint of a client bridge.
        """

        BUFSZ = 8192

        def __init__(self, addrinfo, sibling = None):
            asyncore.dispatcher.__init__(self)
            self._addrinfo = addrinfo;
            self._sibling = None
            self.set_sibling(sibling)
            self._outbuf = ""

        def __str__(self):
            return "Bridge client {0}".format(self._addrinfo)

        def set_sibling(self, sibling):

            if self._sibling != sibling:

                logging.debug("Setting sibling for %s", self)

                if self._sibling is not None:
                    logging.debug("Clearing sibling for %s", self._sibling)
                    self._sibling.set_sibling(None)

                logging.debug("Setting sibling of %s to %s", self, sibling)
                self._sibling = sibling

                if self._sibling is not None:
                    logging.debug("Updating sibling for %s", self._sibling)
                    self._sibling.set_sibling(self)

        def start(self):
            logging.info("%s opening connection.", self)
            self.create_socket(socket.AF_INET, socket.SOCK_STREAM)
            self.connect(self._addrinfo)
            logging.info("%s connected.", self)

        def bridge_recv(self, data):
            self._outbuf = "".join((self._outbuf, data))

        def forward(self, data):
            if self._sibling is not None:
                self._sibling.bridge_recv(data)

        def writable(self):
            return (len(self._outbuf) > 0)

        def handle_write(self):
            sent = self.send(self._outbuf)
            self._outbuf = self._outbuf[sent:]

        def handle_read(self):
            self.forward(self.recv(ClientBridge.Client.BUFSZ))

        def handle_close(self):
            logging.info("%s closing connection.", self)
            raise BridgeClose("One side of the bridge closed {0}.".format(self))


    def __init__(self, addr1, addr2):
        self._client1 = ClientBridge.Client(addr1)
        self._client2 = ClientBridge.Client(addr2, self._client1)
        self._looper = None
        self._exc_info  = None

    def exc_info(self):
        return self._exc_info

    def start(self):
        self._looper = threading.Thread(target=self.run)
        self._looper.setDaemon(True)
        self._looper.start()

    def run(self):

        self._client1.start()
        self._client2.start()

        try:
            asyncore.loop(use_poll=True)

        except BridgeClose as e:
            logging.debug("Lost bridge endpoint; shutting down (%s).", str(e))
            self._exc_info = e

        except Exception as e:
            logging.debug("Caught exception in looper (probably harmless): %s (thread %s)", str(e), threading.currentThread().name)

    def shutdown(self):
        if self._client1 is not None:
            logging.debug("Closing client1 (thread %s)", threading.currentThread().name)
            self._client1.close()
            logging.debug("Client1 closed (thread %s).", threading.currentThread().name)

        if self._client2 is not None:
            logging.debug("Closing client2 (thread %s)", threading.currentThread().name)
            self._client2.close()
            logging.debug("Client2 closed (thread %s).", threading.currentThread().name)


        logging.debug("Client bridge: joining looper (from thread %s).", threading.currentThread().name)
        self._looper.join()
        logging.debug("Client bridge: looper terminated (from thread %s).", threading.currentThread().name)

if __name__ == '__main__':

    logging.basicConfig(level=logging.DEBUG)

    DOOR_SERVER_HOST = "0.0.0.0"
    DOOR_SERVER_PORT = 0xCA7D

    DOOR_REMOTE_ADDR = None

    import argparse
    parser = argparse.ArgumentParser(
        formatter_class=argparse.RawDescriptionHelpFormatter,
        description= """
    doorbridge.py is a simple server for bridging network traffic between a
    single peripheral ("door") and N controllers. Specifically:

     1) It listens on one port (the peripheral port) for a connection from a
        peripheral.

     2) It listens on another port (the controller port) for connections from
        peripherals.

     3) Data received from a connected peripheral port is echoed to all
        connected controllers, unchanged.

     4) Data received on from a controller is sent to the connected peripheral
        (if any), unchanged.

     5) Peripherals and controllers can connect and disconnect at any time
        without affecting the states of other connections.

    Additionally, when passed the -d / --remotedoor param, the the bridge will
    attempt to connect to a remote door that's running its own server.  Note that
    in this mode, if the remote door closes the connection the bridge process will
    terminate.

    Basic operation (pure server):

       1) Start the server

          ./doorbridge.py

       This starts a server that listens on all interfaces on the at port 51837 (0xCA7D)
       for the door, and 51838 (0xCA7E) for controllers.

       2) Connect the peripheral (door) to the server at the peripheral port (51837).
          This can be a simple telnet or socket connection; the protocol is line-oriented
          but but is otherwise agnostic to data.

       3) Connect controllers to the same host, at port 51838 (0xCA7E)

       4) Start sending/receiving traffic.

    Basic operation (server-originated door connection):

          ./doorbridge -d 192.168.0.100:6666

        1) This will start the server with the usual controller port (51838), but will also initiate
           a connection to a remote door listening on 192.168.0.100, port 6666. Modify these values
           as you see fit.

        2) Follow steps (3) and (4) above.

    """
    )

    parser.add_argument("-p",
                        "--port",
                        help="Port number for the door server listener",
                        type=int,
                        default=DOOR_SERVER_PORT)

    parser.add_argument("-a",
                        "--address",
                        help="Address the door server should bind to",
                        default=DOOR_SERVER_HOST)

    parser.add_argument("-d",
                        "--remotedoor",
                        help="Connect to a remote door at the given address and port. Example: -d 192.168.0.100:6666",
                        default=DOOR_REMOTE_ADDR)

    args = parser.parse_args()

    DOOR_SERVER_HOST = args.address
    DOOR_SERVER_PORT = args.port

    DOOR_REMOTE_ADDR = args.remotedoor

    logging.info("Will listen on %s:%s", DOOR_SERVER_HOST, DOOR_SERVER_PORT)


    logging.info("Starting up.")

    doorserver = None
    client_bridge = None

    should_serve = True

    while should_serve:

        # This outer loop automatically restarts the door server in the case that
        # the door disconnects, as long as we still should be serving

        exc_emitters = set()

        try:
            doorserver  = DoorServer((DOOR_SERVER_HOST, DOOR_SERVER_PORT), DoorHandler)
            exc_emitters.add(doorserver)

            connected_ip, connected_port = doorserver.server_address
            logging.info("Door server will bind to %s:%s", connected_ip, connected_port)
            doorserver.start()

            if DOOR_REMOTE_ADDR is not None:

                # We're going to run a bridge between our local door server, and a remote device also running
                # a server.

                DOOR_REMOTE_HOST, DOOR_REMOTE_PORT = DOOR_REMOTE_ADDR.split(":", 2)
                logging.info("Will initiate connection to remote door at %s:%s", DOOR_REMOTE_HOST, DOOR_REMOTE_PORT)
                remote_addrinfo = (DOOR_REMOTE_HOST, int(DOOR_REMOTE_PORT))
                local_addrinfo = (DOOR_SERVER_HOST, int(DOOR_SERVER_PORT))

                client_bridge = ClientBridge(remote_addrinfo, local_addrinfo)
                exc_emitters.add(client_bridge)

                client_bridge.start()

            while should_serve:

                # Every half second, check for exceptions on either the client bridge
                # or the door server (they run in their own threads, so we can't catch
                # them via the usual try/except method here.

                exc_info = (i.exc_info() for i in exc_emitters)
                for e in exc_info:
                    if e is not None:
                        raise(e)

                time.sleep(0.5)

        except KeyboardInterrupt as e:

            # Bail cleanly if we get a ctrl-c

            logging.info("Shutting down due to keyboard interrupt.")
            should_serve = False

            if client_bridge is not None:
                logging.debug("Shutting down client bridge.")
                client_bridge.shutdown()
                logging.debug("Client bridge shutdown complete.")

            if doorserver is not None:
                logging.debug("Shutting down doorserver.")
                doorserver.shutdown()
                logging.debug("Doorserver shutdown complete.")

        except BridgeClose as e:

            logging.error("Shutting down due to exception in child thread: %s.", str(e))
            should_serve = False

            if client_bridge is not None:
                logging.debug("Shutting down client bridge.")
                client_bridge.shutdown()
                logging.debug("Client bridge shutdown complete.")

            if doorserver is not None:
                logging.debug("Shutting down doorserver.")
                doorserver.shutdown()
                logging.debug("Doorserver shutdown complete.")


    logging.info("Exiting.")
    sys.exit(0)

