#!/bin/bash

# pass prev, curr, next as arguments
print_image()
{
if [ $2 == "x" ]
then
  return
fi
# TODO: make a img block with preview image look good
cat >> index.html << EOF
<div class="perfundo" style="width:33%;height:auto;" >
  <a class="perfundo__link" href="#perfundo-$2">
    <img src="${2%.jpg}-small.jpg" alt="$2">
  </a>
  <div id="perfundo-$2" class="perfundo__overlay fadeIn">
    <figure class="perfundo__content perfundo__figure">
      <img src="${2%.jpg}-small.jpg" alt="$2">
      <div class="perfundo__image" style="width:90vh; height:90vw; max-width: 90vw; max-height: 90vh;  background-repeat:no-repeat;  background-color:#7f7f7f; background-image: url($2); background-position:center; background-size:contain;"></div>
    </figure>
    <a href="#perfundo-untarget" class="perfundo__close perfundo__control">close</a>
EOF
if [ $3 != "x" ]
then
cat >> index.html << EOF
    <a class="perfundo__next perfundo__control" href="#perfundo-$3">next</a>
EOF
fi
if [ $1 != "x" ]
then
cat >> index.html << EOF
    <a class="perfundo__prev perfundo__control" href="#perfundo-$1">prev</a>
EOF
fi
cat >> index.html << EOF
  </div>
</div>
EOF
}

# head and foot
cat > index.html << EOF
<!DOCTYPE html>
<html>
<meta name="viewport" content="width=device-width, initial-scale=1">
<link rel="stylesheet" href="perfundo.with-icons.min.css">
<head>
</head>
<body>
<div style="display:flex;flex-wrap: wrap;"/>
EOF
# <link rel=stylesheet type="text/css" href="style.css">
# <link rel=stylesheet type="text/css" href="slides.css">

rm -f *-small.jpg
rm -f modal.txt row.txt
prev=x
curr=x
next=x
for i in $(ls -1 *.jpg)
do
  convert -resize 512x512 -quality 90 $i ${i%.jpg}-small.jpg
  next=$i
  print_image $prev $curr $next
  prev=$curr
  curr=$next
done
print_image $prev $curr x

# ???
# <div class="perfundo">
#   <a class="perfundo__link" href="#perfundo-content">
#     Open the lightbox...
#   </a>
#   <div id="perfundo-content" class="perfundo__overlay bounceIn">
#     <div class="perfundo__content perfundo__html">
#       <h5>Lorem Ipsum</h5>
#       <p>Sadipscing elitr, sed diam nonumy eirmod tempor invidunt ut labore et dolore magna aliquyam erat, sed diam voluptua. At vero eos et accusam et justo duo dolores et ea rebum. Stet clita kasd gubergren, no sea takimata sanctus est Lorem ipsum dolor sit amet.</p>
#       <p>Lorem ipsum dolor sit amet, consetetur sadipscing elitr, sed diam nonumy eirmod tempor invidunt ut labore et dolore magna aliquyam erat, sed diam voluptua. At vero eos et accusam et justo duo dolores et ea rebum. Stet clita kasd gubergren, no sea takimata sanctus est Lorem ipsum dolor sit amet. Lorem ipsum dolor sit amet, consetetur.</p>
#     </div>
#     <a href="#perfundo-untarget" class="perfundo__close perfundo__control">close</a>
#   </div>
# </div>
cat >> index.html << EOF
</div>
</body>
</html>
EOF

# TODO: optionally:
# <!-- Put this before the closing </body> tag (optionally!). -->
# <script src="perfundo.min.js"></script>
# <script>
#   perfundo('.perfundo');
# maybe with arguments:
# perfundo('.perfundo', {
#   disableHistory: true,
#   swipe: true
# });
# </script>
# this is an img entry then:
# <div class="perfundo js-perfundo">
#   <a class="perfundo__link js-perfundo-link" href="#perfundo-img2">
#     <img src="img/img2_s.jpg" alt="Demo image">
#   </a>
#   <div id="perfundo-img2" class="perfundo__overlay fadeIn js-perfundo-overlay">
#     <figure class="perfundo__content perfundo__figure">
#       <img src="img/img2_s.jpg" alt="Demo image">
#       <div class="perfundo__image" style="width: 800px; padding-top: 66.25%; background-image: url(img/img2.jpg);"></div>
#     </figure>
#     <a href="#perfundo-untarget" class="perfundo__close perfundo__control js-perfundo-close">Close</a>
#     <a class="perfundo__next perfundo__control js-perfundo-next" href="#perfundo-img3">Next</a>
#     <a class="perfundo__prev perfundo__control js-perfundo-prev" href="#perfundo-img1">Prev</a>
#   </div>
# </div>
# 
