import uuid

import requests

from multiprocessing import Process

from pprint import pprint

from time import time

def size_test():
    session = requests.session()
    length = 4 * 1024 * 1024  # ~4MB
    data = {"id": "a" * length}
    res = session.post("http://localhost:8080/echo", json=data).json()
    if {"echo": data} != res:
        print("size test: failed")
    else:
        print("size test: ok")
    print()

P = 100  # number of concurrent processes
N = 5  # for each process, the number of sessions
R = 10  # for each session, the number of posts

def test(i):
    global C
    # print(f'starting process {i}')
    for _ in range(N):
        session = requests.session()
        for i in range(1, R + 1):
            session_id = str(uuid.uuid4())
            if {'cnt': i, 'response': {'echo': {'request': {'id': session_id}}}} \
                    != session.post("http://localhost:8080/send", json={"id": session_id}).json():
                print('test failed!')
    # print(f'exiting process {i}')


if __name__ == '__main__':
    size_test()
    # exit()

    processes = [Process(target=test, args=(i, )) for i in range(P)]

    print('starting')

    start = time()

    for p in processes:
        p.start()

    for p in processes:
        p.join()

    end = time()

    print('test ended')
    print('elapsed: ', end - start)
