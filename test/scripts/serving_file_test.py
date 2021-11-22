import requests
import random
from multiprocessing import Process


def test():
    if random.randint(0, 1) == 0:
        resp = requests.get("http://localhost:8080/statics/js/bootstrap.bundle.min.js")
    else:
        resp = requests.get("http://localhost:8080/statics/css/bootstrap.min.css")
    if resp.status_code != 200:
        print(resp)


if __name__ == '__main__':
    print('starting test')
    processes = [Process(target=test) for _ in range(200)]
    for p in processes:
        p.start()
    for p in processes:
        p.join()
    print('end of test')
