from websocket_server import WebsocketServer

steps_remaining = 0
endstop_up   = False
endstop_down = True

# Called for every client connecting (after handshake)
def new_client(client, server):
    print("New client connected and was given id %d" % client['id'])
    server.send_message_to_all("Hey all, a new client has joined us")


# Called for every client disconnecting
def client_left(client, server):
    print("Client(%d) disconnected" % client['id'])


# Called when a client sends a message
def message_received(client, server, message):
    global steps_remaining, endstop_up, endstop_down
    
    if message[:2] == 'up' and endstop_up:
        steps_remaining = 0
        server.send_message_to_all("endstop_up")
    elif message[:4] == 'down' and endstop_down:
        steps_remaining = 0
        server.send_message_to_all("endstop_down")
    
    elif message == 'up_1':     steps_remaining += 1
    elif message == 'up_10':    steps_remaining += 10
    elif message == 'up_100':   steps_remaining += 100; endstop_down = False
    elif message == 'down_1':   steps_remaining -= 1
    elif message == 'down_10':  steps_remaining -= 10
    elif message == 'down_100': steps_remaining -= 100

    if not endstop_up and not endstop_down:
        server.send_message_to_all("endstop_ok")
    print("steps_remaining: %s" % steps_remaining)


PORT=9001
server = WebsocketServer(PORT)
server.set_fn_new_client(new_client)
server.set_fn_client_left(client_left)
server.set_fn_message_received(message_received)
server.run_forever()