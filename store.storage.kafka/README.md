```
wget https://github.com/edenhill/librdkafka/archive/v1.4.4.tar.gz
wget https://github.com/mfontanini/cppkafka/archive/v0.3.1.tar.gz
```

```
sudo sh -c 'echo "/usr/local/lib" >> /etc/ld.so.conf.d/local.conf'

sudo ldconfig
```

Or

```
LD_LIBRARY_PATH=/usr/local/lib
export LD_LIBRARY_PATH
```
