import asyncio
from multiprocessing import Process

import requests
import websockets

import random
import uuid
from time import time

from pprint import pprint

P = 100

def test():

    async def fun(uri):
        session = requests.session()
        n = random.randint(1, 5)
        # print(n)
        for i in range(1, n + 1):
            session_id = str(uuid.uuid4())
            ret = session.post("http://localhost:8080/send", json={"id": session_id}).json()
            # print(ret)
            # print({'cnt': i, 'response': {'echo': {'request': {'id': session_id}}}})
            if {'cnt': i, 'response': {'echo': {'request': {'id': session_id}}}} != ret:
                print('post test failed!')
        sess_id = session.cookies['bsessionid']
        # print("session id:", sess_id)
        async with websockets.connect(uri, extra_headers={'Cookie': f"bsessionid={sess_id}"}) as websocket:
            cnt = int(await websocket.recv())
            # print(cnt)
            if cnt != n:
                print('incorrect cnt')
            m = random.randint(5, 10)
            for _ in range(m):
                session_id = str(uuid.uuid4())
                await websocket.send(session_id)
                ret = await websocket.recv()
                if session_id != ret:
                    print('ws test failed')

    asyncio.get_event_loop().run_until_complete(
        fun("ws://localhost:8080/echo")
    )

if __name__ == '__main__':
    processes = [Process(target=test) for _ in range(P)]

    print('starting')

    start = time()

    for p in processes:
        p.start()

    for p in processes:
        p.join()

    end = time()

    print('test ended')
    print('elapsed: ', end - start)
