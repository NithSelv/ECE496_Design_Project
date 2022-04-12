#!/usr/bin/python3
import subprocess
import time 
import sys 
import socket
import requests
import json
import signal
import os
import threading

# This script is for firing compilation requests to jitserver and trying to keep the load at around 5 requests

c = threading.Lock()
p = threading.Lock()
f = threading.Lock()
ports = []
processes = []
finished = False

def recvmsg(sock):
   global ports
   global finished
   while True:
      try: 
         connection, _ = sock.accept()
         new_port = int(connection.recv(4).decode('utf-8'))
         c.acquire()
         ports.append(new_port)
         c.release()
         p.acquire()
         print("Received a new port: "+str(new_port))
         p.release()
         time.sleep(20)
      except socket.timeout:
         p.acquire()
         print("Socket timed out, closing connection!")
         p.release()
         sock.close()
         f.acquire()
         finished = True
         f.release()
         return
   
   
def sendreq(java_path):
   global ports
   global finished
   global processes
   while True:
      f.acquire()
      if finished:
         f.release()
         return
      f.release()
      c.acquire()
      ports_copy = list(set(ports))
      c.release()
      for x in ports_copy:
         process = sendcmd(java_path, x)
         processes.append(process)
      p.acquire()
      print("Fired a round of requests!")
      p.release()
      time.sleep(10)

def sendcmd(java_path, port):
   args = [java_path]
   args.append("-XX:+UseJITServer")
   args.append("-XX:+JITServerPort="+str(port))
   args.append("-Xjit:count=1"+86*"0")
   args.append("-version")
   process = subprocess.Popen(args, shell=False, stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, close_fds=True)
   return process

def main(java_path):
   sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
   host = socket.gethostname()
   ip = socket.gethostbyname(host)
   sock.bind((host, 2000))
   sock.settimeout(100)
   sock.listen()
   t1 = threading.Thread(target=recvmsg, args=(sock,))
   t2 = threading.Thread(target=sendreq, args=(java_path,))

   t1.start()
   c.acquire()
   while len(ports) <= 0:
      c.release()
      time.sleep(5)
      c.acquire()
   c.release()
   t2.start()

   t1.join()
   t2.join()

   print("Finished sending requests!")
   for x in processes:
      x.terminate()
      x.wait()
      x.kill()
   print("Killed all leftover processes!")

if __name__ == "__main__":
   if len(sys.argv) > 1 and os.path.exists(sys.argv[1]):
      main(sys.argv[1])
   else:
      print("Please specify the path to the java binary!")
