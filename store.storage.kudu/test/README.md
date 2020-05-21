```
cmake .. -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=debug

$ ./example localhost:7051 localhost:7151 localhost:7251 
```

```
# Start cluster
$ docker-compose -f docker/quickstart.yml up

# Check health
$ docker exec -it $(docker ps -aqf "name=kudu-master-1") /bin/bash
$ kudu cluster ksck kudu-master-1:7051,kudu-master-2:7151,kudu-master-3:7251

# Destroy cluster
$ docker-compose -f docker/quickstart.yml down
$ docker rm $(docker ps -aqf "name=kudu")
$ docker volume rm $(docker volume ls --filter name=kudu -q)
```
