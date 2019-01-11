# -*- coding: utf-8 -*-
"""
Created on Tue Jan  1 10:00:37 2019

@author: SF
"""
#%%
from library.telegram import *
from library.intranet  import *

import time
from queue import Queue
from threading import Thread
#%%
def main():
    # Read Telegram file
    telegram = {}
    with open("telegram.txt") as file:
        for line in file:
           (key, val) = line.rstrip('\n').split('=')
           telegram[key] = val
    telegram['authgroup'] = int(val)
    # initialize thread channels
    thcmds = {}
    thcmds['telegram'] = Queue()    
    thcmds['intranet'] = Queue()    
    # prepare threadings
    ths = {}
    ths['telegram'] = telegramobject(cmd_queues = thcmds, token = telegram['token'], botname = telegram['botname'], authgroup = telegram['authgroup'])    
    ths['intranet'] = intranetobject(cmd_queues = thcmds)    
    # initialize threadings
    for key, th in ths.items():     
        th.daemon = True
        th.start()    
    # main loop
    while True: 
        # Data Sharing
        try:    
            ths['telegram'].setparams('devices',   ths['intranet'].getdevices())
        except Exception as e:
            print (e)
#%%
if __name__ == '__main__':
    main()