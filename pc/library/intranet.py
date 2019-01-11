# -*- coding: utf-8 -*-
"""
Created on Wed Jan  9 11:34:40 2019

@author: SF
"""
import sys
from queue import Queue
from threading import Thread
import time

import socket
import json
import subprocess 
import netifaces
#%% Ping API - ping the IP in LAN
class pingobject(Thread):
    def __init__(self, cmd_queues):
        super().__init__()
        self.ip_header, self.port   = self.initparams()
        self.validipset             = set()
        self.invalidipset           = set()
        self.scanip                 = 0                                     # scan ip index [1:255]
        self.timeout                = {'scan': 0.1, 'stable': 5}
        self.tupdate                = time.time()
        self.tstable                = 60                                    # stable mode after 15s no update

    def getvalidip(self):
        return self.validipset
    
    def getinvalidip(self):
        return self.invalidipset
    
    def setinvalidip(self, invalidipset):
        self.invalidipset = invalidipset
    
    def initparams(self):
        gateways        = netifaces.gateways()
        default_gateway = gateways['default'][netifaces.AF_INET][0]
        gateway_ip      = default_gateway.split('.')
        ip_header       = '.'.join(gateway_ip[:3]) + '.'
        return ip_header, 80
            
    def scan(self, iplow):
        if time.time() > self.tupdate + self.tstable:                       # scan offers stable result
            is_scanning = False                                             # Clear the flag
            socket.setdefaulttimeout(self.timeout['stable'])                # use stable timeout
        else:                                                               # scanning mode
            is_scanning = True                                              # set the flag
            socket.setdefaulttimeout(self.timeout['scan'])                  # use scanning timeout
        
        ip = self.ip_header + str(iplow)                                    # compute ip
        if (self.scanip not in self.validipset) or not is_scanning:         # not in valid ip list or not in scanning
            socket_obj = socket.socket(socket.AF_INET, socket.SOCK_STREAM)  # create socket
            result = socket_obj.connect_ex((ip, self.port))                 # ping the ip
            socket_obj.close()                                              # close socket
            if result == 0:                                                 # ping OK
                self.tupdate = time.time()                                  # store timestamp 
                self.validipset.add(ip)                                     # add valid ip to set
            elif ip in self.invalidipset:                                   # ip in invalid ip set
                self.invalidipset.remove(ip)                                # remove invalid ip in set during stable
            
    def run(self):
        """
        Run is the main-function in the new thread. Here we overwrite run
        inherited from threading.Thread.
        """
        self.status = True
        while self.status:   
            self.scanip = self.scanip + 1
            if self.scanip > 255:
                self.scanip = 1
            self.scan(self.scanip)
                
    def stop(self):
        Thread.join(self, None)
#%% Network API
class intranetobject(Thread):
    
    def __init__(self, cmd_queues):
        super().__init__()
        print ('create intranet object')
        self.cmd_queues = cmd_queues
        
        self.ip_header, self.port = self.initparams()
        
        self.invalidip  = set()
        self.validip    = set()
        self.ipindex    = 0
        
        self.devices    = {}
        self.devtimeout = 60
        
        # Ping IP object
        self.thcmds = {}
        self.thcmds['ping'] = Queue()
        # prepare threadings
        self.ths = {}
        self.ths['ping'] = pingobject(cmd_queues = self.thcmds)
        # initialize threadings
        for key, th in self.ths.items():     
            th.daemon = True
            th.start()    
    
    def getdevices(self):
        return self.devices
    
    def initparams(self):
        gateways        = netifaces.gateways()
        default_gateway = gateways['default'][netifaces.AF_INET][0]
        gateway_ip      = default_gateway.split('.')
        ip_header       = '.'.join(gateway_ip[:3]) + '.'
        return ip_header, 80
    
    def iotrequest(self, ip, payload):        
        HOST = ip
        PORT = self.port
        try:
            socket.setdefaulttimeout(30)
            with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
                s.connect((HOST, PORT))
                s.sendall(payload)
                data = s.recv(1024)  
            output = json.loads(data)
        except Exception as e:
            output = None
#            print(e)
        return output
        
    def cycle(self):
        for key, value in self.devices.items():                                             # timeout check
            if time.time() > self.devices[key]['last'] + self.devtimeout:                   # if device lost connect for a predefined period
                del self.devices[key]                                                       # del it from dict
        self.invalidip      = self.ths['ping'].getinvalidip()                               # get invalid ip set from ping object into list
        self.iplist         = list(self.ths['ping'].getvalidip())                           # convert valid ip set from ping object into list
        
        self.ipindex        = self.ipindex + 1
        if self.ipindex >= len(self.iplist):
            self.ipindex    = 0            
 
        if len(self.iplist) > 0 and self.iplist[self.ipindex] not in self.invalidip:   
            ip = self.iplist[self.ipindex]
            if ip in self.devices.keys():                                                   # ip exists
                payload = json.dumps({"status":{}}, separators=(',',':')).encode()          # read the status only
                status = self.iotrequest(ip, payload)                                       # 
                if status:                                                                  # status is valid
                    self.devices[ip]['status']  = status                                    # store status
                    self.devices[ip]['last']    = time.time()                               # update connection time
                    print (ip, status)
#            if not config and not params and not status:  
            else:                                                                           # ip not exist
                payload = json.dumps({"config":{}}, separators=(',',':')).encode()
                config = self.iotrequest(ip, payload)
                if not config:                                                              # Check only config request, not wasting time
                    self.invalidip.add(ip)                                                  # add ping-able ip to invalid ip set
                    self.ths['ping'].setinvalidip(self.invalidip)                           # update invalid ip set in ping object
                    print (len(self.devices.keys()))
                else:                                                                       # config exists
                    payload = json.dumps({"params":{}}, separators=(',',':')).encode()      # request params
                    params = self.iotrequest(ip, payload)                                   #
                    payload = json.dumps({"status":{}}, separators=(',',':')).encode()      # request status
                    status = self.iotrequest(ip, payload)                                   #
                    self.devices[ip] = {'config':config, 'params':params, 'status':status, 'last':time.time()}
                    print (len(self.devices.keys()))
                    
        
    def run(self):
        """
        Run is the main-function in the new thread. Here we overwrite run
        inherited from threading.Thread.
        """
        self.status = True
        while self.status:       
            if self.cmd_queues['intranet'].empty():
                self.cycle()
                
            else:   
                self.getprompt()
                self.cmd_queues['intranet'].task_done()  # unblocks prompter 
                
    def stop(self):
        self.status = False
        Thread.join(self, None)
        
    def getprompt(self):

        commands = self.cmd_queues['intranet'].get()
        try:       
            for ip, command in commands.items():
                print (ip, command)
                payload = json.dumps(command, separators=(',',':')).encode()
                reply = self.iotrequest(ip, payload)
                if reply:
                    for key, value in command.items():
                        self.devices[ip][key]       = reply
                        self.devices[ip]['last']    = time.time()
        except Exception as e:
            print(f'Command `{cmds}` is unknown: ', e)
            
    # Report Params
    def setparams(self, name, val):
        return None
#%%