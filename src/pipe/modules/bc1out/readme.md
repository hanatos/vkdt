# output bc1 compressed files

this is useful for thumbnails, which can be stored
compactly on disk and in memory, and displayed directly
from this format.

use libsquish to compress the input.
(TODO: also try binomial.info's basis crunch https://github.com/BinomialLLC/crunch)

try libsquish + zlib to see if blocks can be compressed even more.

unfortunately libsquish comes with a c++ interface.
