# i-mcraw: load raw video from the motioncam app

this input module reads `.mcraw` files written by the motioncam android app.

## connectors

* `output` the 16-bit single channel raw data

## parameters

* `filename` the file name of the mcraw video to load
* `noise a` the gaussian noise parameter
* `noise b` the poissonian noise parameter
* `start` start frame, to trim the video from the left (use frame count to trim from the right)
* `black` the four black levels. leave 0 to read from metadata
* `white` the white level of the data. leave 0 to read from metadata
