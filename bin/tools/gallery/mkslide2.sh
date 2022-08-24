#!/bin/bash
# bash script to create html/css web gallery from list of jpg images in this directory.

# pass prev, curr, next as arguments
print_image()
{
if [ $2 == "x" ]
then
  return
fi
cat >> index.html << EOF
<div class="gallery">
  <a class="gallery__link" href="#gallery-$2">
    <div class="gallery__thumbnail" style="background-image:url(${2%.jpg}-small.jpg);"></div>
  </a>
  <div id="gallery-$2" class="gallery__overlay fadeIn">
    <figure class="gallery__content gallery__figure">
      <img src="${2%.jpg}-small.jpg" alt="$2">
      <div class="gallery__image" style="background-image: url($2);"></div>
    </figure>
    <a href="#gallery-untarget" class="gallery__close gallery__control">close</a>
EOF
if [ $3 != "x" ]
then
cat >> index.html << EOF
    <a class="gallery__next gallery__control" href="#gallery-$3">next</a>
EOF
fi
if [ $1 != "x" ]
then
cat >> index.html << EOF
    <a class="gallery__prev gallery__control" href="#gallery-$1">prev</a>
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
<link rel="stylesheet" href="gallery.css">
<head>
</head>
<body>
<div class="gallery__grid">
EOF

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

cat >> index.html << EOF
</div>
<p class="footnote">web gallery created with $(vkdt --version), images (c) their owners</p>
</body>
</html>
EOF
