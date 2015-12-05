#!/usr/bin/env python

import SocketServer
import logging
import sys
import re
import threading


# VERY simple chat-like server for unicasting/multicasting messages
# to connected clients. After starting, connec to its port. The protocol
# looks like this:
#
# :r <type> <name> registers a client.  This registers a door named "ray":
#
#     :r door ray
#
# :m <predicate> <msg> sends a message.as
#
#     This delivers the message "one two three four" to all clients:
#
#         :m .* one two three four
#
#     This delivers the message "one two three four" to all doors:
#
#         :m ^door_ one two three four
#
#     This delivers the message "one two three four" to a door named Ray
#
#         :m ^door_ray$ one two three four
#
# :c sends a command to the server.
#
#     This lists all doors (space-delimited; trailing newline:
#
#         :c list
#
#     This (coming from a client) disconnects that client:
#
#         :c d
#
#     This disconnect all clients matching a predicate:
#
#         :c d .*
#         :c d ^door_

class Message(object):
    """
    Basic message type passed around among remotes.
    """

    def __init__(self, src_remote, dest_predicate, body):
        self._src_remote = src_remote
        self._dest_predicate = dest_predicate
        self._body = body

    def __str__(self):
        return "Message src={0} dest_pattern={1} body={2}".format(str(self.get_src_remote()),
                                                                  self.get_dest_predicate(), self.get_body())

    def get_src_remote(self):
        """
        Get the RemoteClient that was the source of this message.
        """
        return self._src_remote

    def get_dest_predicate(self):
        """
        Get the destination name pattern that will be used to match registry keys to route this message.
        """

        return self._dest_predicate

    def get_body(self):
        return self._body

    @staticmethod
    def SimpleParse(src_remote, data):


        predicate = r'.*'
        cmd = None

        if data.startswith('t:'):
            m = re.match(r'^t\:(\S*)\s+(.*$)')
            if not m:
                msg = "Parse error on message from {0} data: {1}".format(src_remote, data)
                raise ValueError(msg)
            predicate = m.group(1)
            cmd = m.group(2)
        else:
            cmd = data

        return Message(src_remote, predicate, cmd)


class RemoteClient(object):

    """
    Base interface for remote clients.
    """

    class RemoteClientDelegate(object):
        """
        Delegate protocol for the remote client
        """

        def handle_message_data(self, message_data):
            """
            Handle an incoming message.
            """
            raise NotImplementedError("handle_message is abstract.")

        def handle_client_closed(self, client):
            """
            Handle a remote client closing. After receiving this event, this client must no
            longer receive any messages.
            """
            raise NotImplementedError("handle_client_closed is abstract.")

        def handle_client_opened(self, client):
            """
            Handle a remote client opening.
            """

            raise NotImplementedError("handle_client_opened is abstract.")

    def register(self, dispatcher, remote_type, remote_name):
        self._set_remote_name(remote_name)
        self._set_remote_type(remote_type)
        self._set_dispatcher(dispatcher)
        logging.debug("Registered remoteclient: %s", str(self))

    def _set_dispatcher(self, dispatcher):
        self._dispatcher = dispatcher
        dispatcher.register_remote(self)

    def _set_remote_type(self, remote_type):
        self._remote_type = remote_type

    def get_remote_type(self):
        try:
            return self._remote_type
        except AttributeError:
            self._remote_type = None
            return self._remote_type

    def _set_remote_name(self, remote_name):
        self._remote_name = remote_name

    def get_remote_name(self):
        try:
            return self._remote_name
        except AttributeError:
            self._remote_name = None
            return self._remote_name

    def set_delegate(self, delegate):
        if not isinstance(delegate, RemoteClient.RemoteClientDelegate):
            raise TypeError("{0} is not a RemoteClientDelegate".format(str(delegate)))

        self._delegate = delegate

    def get_delegate(self):
        try:
            return self._delegate
        except AttributeError:
            self._delegate = None
            return self._delegate


    def send(self, message):
        raise NotImplementedError("send is abstract.")




class RemoteStreamClient(SocketServer.StreamRequestHandler, RemoteClient):

    def __init__(self, *args, **kwargs):
        SocketServer.StreamRequestHandler.__init__(self, *args, **kwargs)
        logging.debug("RemoteStreamClient initialized: %s", str(self))

    def __str__(self):
        return "name:{0} type:{1} address:{2} delegate:{3}".format(self.get_remote_name(), self.get_remote_type(), self.client_address, self.get_delegate())

    def send(self, message):

        """
        Takes a Message object, and forwards the body to the client.
        """

        logging.info("SEND to={0} data={1}".format(str(self), str(message)))
        self.wfile.write(message.get_body())
        self.wfile.flush()

    def handle(self):

        """
        Handle a single incoming message
        """

        # HELLO type name

        handshake = self.rfile.readline().strip()
        (hello, client_type, client_name) = handshake.split(" ", 3)
        self.register(self.server, client_name, client_type)

        for message_data in self.rfile:

            message_data = message_data.strip()

            if self.get_delegate() is None:
                logging.warn("No delegate for message: " + str(message_data))
            else:
                logging.info("HANDLE message: " + str(message_data))
                self.get_delegate().handle_message(message_data, self)


class RemoteClientProxy(RemoteClient.RemoteClientDelegate):
    """
    Superclass for Door and Controller marker/holders.
    """

    def __init__(self, remote_client, name):
        self._remote_client = remote_client
        self._remote_client.set_delegate(self)
        self._name = name

    def __str__(self):
        return "{0}:{1}".format(self.get_registry_key(), self.get_address())

    def get_client(self):
        return self._remote_client

    def get_address(self):

        if self._remote_client is None:
            return None

        return self._remote_client.client_address

    @classmethod
    def make_registry_key(klass, individual_name):

        if klass.__name__ == "RemoteClientProxy":
            logging.warning("Calling make_registry_key on abstract super")

        return "{0}_{1}".format(klass.__name__, individual_name)

    def get_registry_key(self):
        return self.__class__.make_registry_key(self.get_name())

    def get_name(self):
        return self.get_client().get_remote_name()

    def send(self, message):

        if self._remote_client is None:
            logging.warning("No client set for client proxy {0}", str(self))
            return

        self._remote_client.send(message)

    # remote client delegate

    def handle_message(self, data, client):

        logging.info("HANDLE_MESSAGE message=%s from=%s", data, client)

    def handle_client_closed(self, client):

        logging.info("HANDLE_CLIENT_CLOSED client=%s".format(client))

    def handle_client_opened(self, client):

        logging.info("HANDLE_CLIENT_OPENED client={0}".format(client))


class Door(RemoteClientProxy):
    """
    "The remote door.
    """

    def open(self, sender):
        logging.info("Opening door at behest of {0}".format(str(sender)))
        self.get_client().send("open_door\n")


class Controller(RemoteClientProxy):
    """
    The remote handset.
    """

    def notify(self, message):
        self.get_client().send("notify {0}".format(str(message)))


class Registry(object):
    def __init__(self):
        self._registry = dict()

    def register(self, remote_client):

        if not isinstance(remote_client, RemoteClient):
            raise TypeError("%s is not a RemoteClient subclass", remote_client)

        remote_type = remote_client.get_remote_type()

        if "door" == remote_type:
            proxy = Door(remote_client, None)
        elif "controller" == remote_type:
            proxy = Controller(remote_client, None)
        else:
            logging.warning("Unrecognized remote_type %s for client %s", remote_type, remote_client)
            proxy = None

        if proxy is None:
            return

        key = proxy.get_registry_key()

        self._registry[key] = proxy

    def remotes(self, predicate=None):
        """
        Predicate is a very regex on keys filter on keys.
        Returns an iterator over matching RemoteProxies. Not sorted.
        """

        if predicate is None:
            return (self._registry[key] for key in self._registry.keys())

        r = re.compile(predicate)

        return (self._registry[key] for key in self._registry.keys() if r.match(key))

    def get_proxy(self, key):
        return self._registry.get(key)


class Dispatcher(SocketServer.ThreadingMixIn, SocketServer.TCPServer):

    def __init__(self, *args, **kwargs):
        SocketServer.TCPServer.__init__(self, *args, **kwargs)
        self._registry = Registry()


    def register_remote(self, remote_client):
        try:
            self._registry.register(remote_client)
        except TypeError as e:
            logging.warning("Can't register remote client: %s", e)

    def remotes(self, predicate):
        return self._registry.remotes(predicate)

    def send(self, message):

        """
        Send the given message.  Will be sent to all clients
        that match the dest_predicate on the given message.
        """

        for remote_proxy in self.remotes(message.get_dest_predicate()):

            try:
                remote_proxy.send(message)

            except IOError as e:
                print >> sys.stderr, "Poo! %s" % e


if "__main__" == __name__:

    logging.basicConfig(level=logging.INFO)

    logging.info("Starting up.")
    DEFAULT_HOST = "0.0.0.0"
    DEFAULT_PORT = 0xD06E

    requested_host = DEFAULT_HOST
    requested_port = DEFAULT_PORT
    dispatcher = Dispatcher((requested_host, requested_port), RemoteStreamClient)

    connected_ip, connected_port = dispatcher.server_address

    logging.info("Server bound on %s:%s", connected_ip, connected_port)

    server_thread = threading.Thread(target=dispatcher.serve_forever)
    server_thread.daemon = True
    server_thread.start()

    logging.info("Server running in thread: %s", server_thread.name)
    server_thread.join()




        
