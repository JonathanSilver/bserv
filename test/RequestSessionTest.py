import uuid
import requests
import random
from multiprocessing import Process
from time import time
from pprint import pprint

def size_test():
    session = requests.session()
    length = 4 * 1024 * 1024  # ~4MB
    data = {"id": "a" * length}
    res = session.post("http://localhost:8080/echo", json=data).json()
    if {"id": "echo", "obj": data} != res:
        print("size test: failed")
    else:
        print("size test: ok")
    print()

P = 200  # number of concurrent processes
N = 20  # for each process, the number of posts

def test():
    session = requests.session()
    if {"hello": "world"} \
            != session.get("http://localhost:8080/hello").json():
        print('test failed!')
    if {"id": "get", "val": 0} \
            != session.get("http://localhost:8080/get").json():
        print('test failed!')
    cnt = 0
    for _ in range(N):
        session_id = str(uuid.uuid4())
        if random.randint(0, 1) == 0:
            if {"id": "echo2", "str": session_id} \
                    != session.get(f"http://localhost:8080/echo/{session_id}").json():
                print('test failed!')
        else:
            data = {"id": session_id}
            if {"id": "echo", "obj": data} \
                    != session.post("http://localhost:8080/echo", json=data).json():
                print('test failed!')
            cnt += 1
        if {"id": "get", "val": cnt} \
                != session.get("http://localhost:8080/get").json():
            print('test failed!')


if __name__ == '__main__':
    size_test()

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
