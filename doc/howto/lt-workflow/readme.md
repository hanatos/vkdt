# how to efficiently rate and discard images in lighttable mode

when coming back from an event or field trip, photographers are often confronted
with thousands of images and the task to select a few keepers for further postprocessing.
this can be time-consuming and in general daunting because of the sheer amount of work.

the following presents an example workflow that has been proposed by your friendly
darktable community.

## import from flash cards

the `files` view in `vkdt` provides some basic tools to mount/unmount flash
cards and run (several parallel) import jobs in the background which copy files
from the mounted locations to a specified folder on your harddrive. if this
isn't for you, you can just copy and admin your images as you prefer (for
instance using [rapid photo downloader](https://damonlynch.net/rapid/)).

you can already start looking at your (already copied) images in lighttable or
darkroom views while `vkdt` is still copying the rest.

## tools for rating

works in both lighttable and darkroom:
star ratings, colour labels, upvote, downvote
tab in darkroom mode for fullscreen inspection

upvote/downvote: iterative strategy, repeatedly go over list of upvoted images
until only a few are left.

## techniques against fatigue

especially when reviewing a large number of very similar images, it can be
tiresome to work through the list. it is therefore essential to have strategies
against this reviewing fatigue. here are some ideas.

### small groups
reducing the number of images visible in the current collection has a big
psychological impact. going through a few tens of images seems much more a
doable task than looking at say 1000. it is thus a good idea to break down
the long list into smaller groups.

go through your images day by day, button support in the collection interface.
note that you'll have to sort your collection by date for this to work.

if a single day is not fine grained enough, or there are many very similar
photos for a different reason, assign such groups a temporary colour label,
filter by this label, and then select the best shot.

### randomise the work
use 1 star to mean: did not look at it yet, unrated
use upvote and downvote to process parts
you can then scroll over the list quickly and proceed with the rating of parts of the unrated list that currently catch your interest.

### casual interfaces
use keyboard shortcuts

use gamepad interface for casual/different approach, maybe while sitting on a couch with a larger monitor (projector, television?).

## export
use `o-web` export in conjunction with `vkdt gallery`
