#!/usr/bin/python

from socket import *
import sys
import random
import os
import time

if len(sys.argv) < 6:
    sys.stderr.write('Usage: %s <ip> <port> <#trials>\
            <#writes and reads per trial>\
            <#connections> \n' % (sys.argv[0]))
    sys.exit(1)

serverHost = gethostbyname(sys.argv[1])
serverPort = int(sys.argv[2])
numTrials = int(sys.argv[3])
numWritesReads = int(sys.argv[4])
numConnections = int(sys.argv[5])

if numConnections < numWritesReads:
    sys.stderr.write('<#connections> should be greater than or equal to <#writes and reads per trial>\n')
    sys.exit(1)

socketList = []

RECV_TOTAL_TIMEOUT = 0.1
RECV_EACH_TIMEOUT = 0.01

#RECV_EACH_TIMEOUT = 1

for i in xrange(numConnections):
    s = socket(AF_INET, SOCK_STREAM)
    s.connect((serverHost, serverPort))
    socketList.append(s)


GOOD_REQUESTS = [
                # 'GET / HTTP/1.1\r\n\r\n',
                # 'GET / HTTP/1.1\r\n\r\n\r\n\r\n',
                # 'GET / HTTP/1.1\r\n\r\n\r\n',
                #'GET / HTTP/1.1\r\nUser-Agent: 441UserAgent/1.0.0\r\n\r\n',
                #'GET / HTTP/1.1\r\nUser-Agent: blablabla\r\n blablabla\r\n blablabla\r\n HiIAmANewLine\r\n\r\n',
                'GET /~prs/15-441-F15/ HTTP/1.1\r\nHost: www.cs.cmu.edu\r\nConnection: keep-alive\r\nAccept: text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*;q=0.8\r\nUser-Agent: Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/39.0.2171.99 Safari/537.36\r\nAccept-Encoding: gzip, deflate, sdch\r\nAccept-Language: en-US,en;q=0.8\r\n\r\n',
]
BAD_REQUESTS = [
    # 'GET /\r HTTP/1.1\r\nUser-Agent: 441UserAgent/1.0.0\r\n\r\n', # Extra CR
    # 'GET / HTTP/1.1\r\nUser-Agent: \r441UserAgent/1.0.0\r\n\r\n', # Extra CR
    # 'GET\r / HTTP/1.1\r\nUser-Agent: 441UserAgent/1.0.0\r\n\r\n', # Extra CR
    # 'GET\n / HTTP/1.1\r\nUser-Agent: 441UserAgent/1.0.0\r\n\r\n', # Extra tab
    # '\nGET / HTTP/1.1\r\nUser-Agent: 441UserAgent/1.0.0\r\n\r\n', # Extra CR
    # 'GET / HTTP/1.1\r\nUser-Agent: 441UserAgent/1.0.0\n\r\n\r\n', # Extra CR
    # 'GET / HTTP/1.1\nUser-Agent: 441UserAgent/1.0.0\r\n\r\n',     # Missing CR
    # 'GET / HTTP/1.1\rUser-Agent: 441UserAgent/1.0.0\r\n\r\n',     # Missing LF
    # # 'GET / HTTP/1.1\r\nUser-Agent: \r\n\r\n',     # Incomplete header
    # 'GET / HTTP/\r\n\r\n',     # Incomplete request line
    # 'GET / HTT\r\n\r\n',     # Incomplete request line
    # '\r\nGET / HTTP/1.1\r\nUser-Agent: blablabla\r\nblablabla\r\nblablabla\r\n HiIAmANewLine\r\n\r\n', #from recitation
    # 'GET / HTTP/1.1\r\nUser-Agent: 441UserAgent/1.0.0\r\n\r\n',
]

BAD_REQUEST_RESPONSE = 'HTTP/1.1 400 Bad Request\r\n\r\n'

for i in xrange(numTrials):
    socketSubset = []
    randomData = []
    randomLen = []
    socketSubset = random.sample(socketList, numConnections)
    for j in xrange(numWritesReads):
        random_index = random.randrange(len(GOOD_REQUESTS) + len(BAD_REQUESTS))
        if random_index < len(GOOD_REQUESTS):
            random_string = GOOD_REQUESTS[random_index]
            randomLen.append(len(random_string))
            randomData.append(random_string)
            print("sent good request", random_string, len(random_string), "done")
        else:
            random_string = BAD_REQUESTS[random_index - len(GOOD_REQUESTS)]
            randomLen.append(len(BAD_REQUEST_RESPONSE))
            randomData.append(BAD_REQUEST_RESPONSE)
            print("sent bad request" , random_string, len(random_string), "done")
        socketSubset[j].send(random_string)

    for j in xrange(numWritesReads):
        data = socketSubset[j].recv(randomLen[j])
        start_time = time.time()
        while True:
            if len(data) == randomLen[j]:
                break
            socketSubset[j].settimeout(RECV_EACH_TIMEOUT)
            data += socketSubset[j].recv(randomLen[j])
            if time.time() - start_time > RECV_TOTAL_TIMEOUT:
                break
        if data != randomData[j]:
            sys.stderr.write("Error: Data received is unexpected! \n")
            print("expected ", randomData[j], "recieved so far ",  data)
            sys.exit(1)

for i in xrange(numConnections):
    socketList[i].close()

print "Success!"
