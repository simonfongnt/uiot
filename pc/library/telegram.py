# -*- coding: utf-8 -*-
"""
Created on Thu Sep 27 11:02:54 2018
Telegram Command List:
control - Select Mode
list - Show all Devices
status - Device's Status
devparams - Device's params
rename - Change Device's Name
count - Number of Devices
get - Read a Param
set - Update a Param

@author: SF
"""
#%%
import telegram
from telegram.error import NetworkError, Unauthorized
import time
from threading import Thread
import datetime
import pandas as pd
import numpy
import re
import collections
import urllib
#%% Telegram API
class telegramobject(Thread):
    
    def __init__(self, cmd_queues, token, botname, authgroup):
        super().__init__()
        print ('create telegram object')
        self.cmd_queues = cmd_queues
        
        self.update_id = None
        self.authgroup = authgroup
#        self.authgp = 
#        self.authid = 
        self.TOKEN = token # 
        self.botname = botname
        # Telegram Bot Authorization Token
        self.bot = telegram.Bot(self.TOKEN)
        # get the first pending update_id, this is so we can skip over it in case
        # we get an "Unauthorized" exception.
        try:
            self.update_id = self.bot.get_updates()[0].update_id
        except IndexError:
            self.update_id = None
        
        self.devices    = {}
        self.devnames   = {}
    
    # convert from message array to json
    def parser(self, jarr):
        print (jarr)
        json = jarr[-1]
        for root in reversed(jarr[:-1]):
            json = {root:json}
            print (json)
        return json   
    
    # get IP by device's name
    def getipbyname(self, name):
        for key, value in self.devnames.items():
            if name == value:
                return key
        return None
    
    # codec for intranet object communcation        
    def codec(self, msg):
        command = {}
        messages = msg.split(' ')
        if messages[0] == 'rename':
            command[messages[1]] = {'config':{'name':messages[2]}}
        elif messages[0] == 'set':
            try:
                messages[-1] = int(messages[-1])                                # convert to int
            except Exception as e:
                print (e)
                pass
            command[messages[1]] = self.parser(messages[2:])
        elif messages[0] == 'control':                                          # control command
            ip = self.getipbyname(messages[1])
            if messages[2] == 'Smart':
                command[ip] = {'params':{'smart':{'on':1}}}
            elif messages[2] == 'On':
                command[ip] = {'params':{'smart':{'on':0},'on':1}}
            elif messages[2] == 'Off':
                command[ip] = {'params':{'smart':{'on':0},'on':0}}            
        self.cmd_queues['intranet'].put(command)
        
    # get node based on provided path in telegram message        
    def getnode(self, root, paths):
        last = root
        if len(paths) > 1:            
            for node in paths[1:]:
                last = last[node]
                if not isinstance(last, collections.Mapping):
                    break
        return last
    
    # get value based on provided path in telegram message        
    def getvalue(self, root, paths):
        last = root
        if len(paths) > 1:            
            for node in paths[1:]:
                last = last[node]
        return last
    
    # condition check for incoming packet        
    def complie(self, msg):
        kb = []
        text            = msg
        messages        = msg.split(' ')
        if messages[0] == 'rename':
            text = msg
            if len(messages) >= 4:
                text = '(No Space in Name?)'
            elif len(messages) == 3:
                if messages[1] in self.devices.keys():
                    self.codec(msg)
                else:
                    text = 'Invalid IP'
            else:
                for ip, info in self.devices.items():
                    kb.append([telegram.KeyboardButton(ip)])
        elif messages[0] == 'control':                                           # control command
            text = msg
            if len(messages) == 1:
                for ip, info in self.devices.items():
                    kb.append([telegram.KeyboardButton(self.devices[ip]['config']['name'])])
            elif len(messages) > 2:
                if (
                        messages[1] in self.devnames.values()                            
                    and (
                            messages[2] == 'Smart'
                         or messages[2] == 'On'
                         or messages[2] == 'Off'
                         )
                    ):
                    self.codec(msg)
                else:
                    text = '(Invalid devices or params)'
            else:
                kb = [[
                        telegram.KeyboardButton("Smart")
                       ],
                       [
                        telegram.KeyboardButton("On"),
                        telegram.KeyboardButton("Off")
                       ]]
        elif messages[0] == 'set' or messages[0] == 'get':                          # prepare keyboard
            text = msg
            nodes = self.getnode(self.devices, messages) 
            if isinstance(nodes, collections.Mapping):                              # last node is still arr or dict 
                for key, node in nodes.items():                                     # keyboard with all nodes at that level
                    kb.append([telegram.KeyboardButton(key)])               
            else:                                                                   # last node is value
                if messages[0] == 'set':
                    self.codec(msg)                                                 # transmit to intranet class
                else:
                    kb = []
                    text = self.getvalue(self.devices, messages)
        elif messages[0] == 'devparams':                                            # prepare keyboard
            text = msg
            if len(messages) == 1:
                for ip, info in self.devices.items():
                    kb.append([telegram.KeyboardButton(self.devices[ip]['config']['name'])])
            elif len(messages) >= 2:
                ip = self.getipbyname(messages[1])
                text = self.devices[ip]
        elif messages[0] == 'count':
            text = len(self.devices)
        elif messages[0] == 'list':
            text = ""
            for key, node in self.devices.items():
                text = text + "%s %s@%s\n"%(node['config']['name'], node['config']['type'], key)
        elif messages[0] == 'status':
            text = ""
            for key, node in self.devices.items():
                if node['config']['type'] == 'LIGHTING':
                    if node['params']['smart']['on'] == 1:
                        mode = 'Smart'
                    elif node['params']['on'] == 1:
                        mode = 'On'
                    else:
                        mode = 'Off'
                    text = text + "%s: %s\n"%(node['config']['name'], mode)
                    for key, value in node['status'].items():
                        text = text + "%s:%i\n"%(key, value)
                                    
        # keyboard?
        reply_markup    = None                                                      # assume no makeup
        if len(kb) > 0:                                                             # valid keyboard
            reply_markup = telegram.ReplyKeyboardMarkup(                            # prepare keyboard for telegram msg
                    kb, 
                    resize_keyboard = True,
                    one_time_keyboard  = True,
#                    selective = True
                    )
        else:                                                                       # close keyboard
            reply_markup = telegram.ReplyKeyboardRemove()
        # Complete Reply Message
        self.bot.send_message(chat_id = self.authgroup,                             # send telegram msg
                              text = text,
                              reply_markup = reply_markup
                              )            
    
    def listen(self):
        """Echo the message the user sent."""
#        global update_id
        # Request updates after the last update_id
        for update in self.bot.get_updates(offset = self.update_id, timeout=10):    # check if update available
            self.update_id = update.update_id + 1                                   # increment update_id
            try:
                if update.message.chat.id == self.authgroup:                        # check if message coming from predefined group
                    self.chat_id = update.message.chat.id                           # store chat id
                    input_msg = update.message.text                                 # retrieve incoming message
                    try:
                        # entities exists and type is bot_command 
                        if input_msg.endswith(self.botname):                        # check if the text ends with bot's name
                            input_msg = input_msg[:-len(self.botname)]              # /command@constituentsBot -> /command
                        if update.message.entities and update.message.entities[0].type == 'bot_command':
                            input_msg = input_msg[1:]                               # /command -> command
                        if (
                                update.message.reply_to_message                     # check if message consists of reply
                            and update.message.reply_to_message.text                # check if text available in reply
                                ):
                            # combine both reply and incoming message for processing
                            input_msg = update.message.reply_to_message.text + ' ' + input_msg
                        self.complie(input_msg)                         # complie the message
                    except Exception as e:
                        print ('invalid command: ', e)
            except Exception as e:
                print (e)
    
    def cycle(self):    
        try:
            self.listen()
        except NetworkError:
            time.sleep(1)
        except Unauthorized:
            # The user has removed or blocked the bot.
            self.update_id += 1        
        for ip, info in self.devices.items():
            try:
                self.devnames[ip] = info['config']['name']
            except Exception as e:
                print (e)
        
    def run(self):
        """
        Run is the main-function in the new thread. Here we overwrite run
        inherited from threading.Thread.
        """
        self.status = True
        while self.status:       
            if self.cmd_queues['telegram'].empty():
                self.cycle()
                time.sleep(1)                           # optional heartbeat
            else:
                self.getprompt()
                self.cmd_queues['telegram'].task_done()  # unblocks prompter
                
    def stop(self):
        self.status = False
        Thread.join(self, None)
    # process input message from other program
    def getprompt(self):
        try:
            while not self.cmd_queues['telegram'].empty():
                cmds = self.cmd_queues['telegram'].get()#
        except Exception as e:
            print(f'Command `{cmds}` is unknown: ', e)
    # Report Params
    def setparams(self, name, val):       
        if name == 'devices':    
            self.devices    = val