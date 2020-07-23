# 10 bit display tester

this is just a simple module to render a gradient [0, 0.5]. it can be used to
test whether your x is setup to render 10 bit/channel (i.e. 30 bit per pixel)
correctly. you can run it with the example config:

```
$ ./vkdt examples/test10b.cfg
```

you should observe some banding on 8-bit and a smooth gradient in 10-bit.
zooming in can help discover artifacts.
