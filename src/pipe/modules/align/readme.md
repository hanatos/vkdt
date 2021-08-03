# image alignment

this module is an implementation of [Johannes Hanika, Lorenzo Tessari, and
Carsten Dachsbacher, Fast temporal reprojection without motion vectors, Journal
of Computer Graphics Techniques, 2021](https://jo.dreggn.org/home/2021_motion.pdf).
it can be useful to align consecutive frames in real-time renders, as well as
to stack hand-held short exposure photography for low-light or hdr.

## connectors

* `alignsrc`  a feature map of the to-be-aligned image
* `aligndst`  a feature map to be aligned to (these will stay static)
* `input`  the input image pixels which will be warped
* `output` the warped output image
* `mask`   the error mask (output)

plus debug channels

## parameters

the parameters are as described in the paper.

* `merge_k` increase this to give the error mask more weight (i.e. reduce potential misalignment artifacts)
* `merge_n` increase this to instruct the error mask that small errors are likely noise and can be ignored
* `blur0`   these blur parameters control the size of the feature detection context (patch size) on different scales. 0 is the finest and 3 the coarsest.
* `blur1`
* `blur2`
* `blur3`


## examples

noisy input vs. aligned and denoised:  
<div class="compare_box">
<textarea readonly style="background-image:url(img_0000.jpg)"></textarea>
<textarea readonly style="background-image:url(img_0003.jpg)"></textarea>
</div>

denoise pre-demosaic without alignment:  
<div class="compare_box">
<textarea readonly style="background-image:url(img_0001.jpg)"></textarea>
<textarea readonly style="background-image:url(img_0003.jpg)"></textarea>
</div>

denoise post-demosaic without alignment:  
<div class="compare_box">
<textarea readonly style="background-image:url(img_0002.jpg)"></textarea>
<textarea readonly style="background-image:url(img_0003.jpg)"></textarea>
</div>

align and denoise post-demosaic:  
<div class="compare_box">
<textarea readonly style="background-image:url(img_0004.jpg)"></textarea>
<textarea readonly style="background-image:url(img_0003.jpg)"></textarea>
</div>

denoise individual inputs and align then:  
<div class="compare_box">
<textarea readonly style="background-image:url(img_0005.jpg)"></textarea>
<textarea readonly style="background-image:url(img_0003.jpg)"></textarea>
</div>

denoise individual inputs and align then vs. single-image pre-demosaic denoise:  
<div class="compare_box">
<textarea readonly style="background-image:url(img_0001.jpg)"></textarea>
<textarea readonly style="background-image:url(img_0005.jpg)"></textarea>
</div>

align and denoise post-demosaic vs. single-image post-demosaic denoise:  
<div class="compare_box">
<textarea readonly style="background-image:url(img_0002.jpg)"></textarea>
<textarea readonly style="background-image:url(img_0004.jpg)"></textarea>
</div>

