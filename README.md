# QuickMedia
Native clients of websites with fast access to what you want to see. [Demo with manga](https://beta.lbry.tv/quickmedia_manga-2019-08-05_21.20.46/7).
Press ctrl+t to when hovering over a manga chapter to start tracking manga after that chapter. This only works if AutoMedia is installed and
accessible in PATH environment variable.
# Dependencies
## Compile
See project.conf \[dependencies].
## Runtime
### Required
curl needs to be downloaded for network requests.
### Optional
youtube-dl needs to be installed to play videos from youtube.
# TODO
Fix x11 freeze that sometimes happens when playing video.
If a search returns no results, then "No results found for ..." should be shown and navigation should go back to searching with suggestions.
Give user the option to start where they left off or from the start.
For manga, view the next chapter when reaching the end of a chapter.
Make network requests asynchronous to not freeze gui when navigating. Also have loading animation.
Retain search text when navigating back.
Disable ytdl_hook subtitles. If a video has subtitles for many languages, then it will stall video playback for several seconds
until all subtitles have been downloaded and loaded.
Figure out why memory usage doesn't drop much when killing the video player. Is it a bug in proprietary nvidia drivers on gnu/linux?
Add grid-view when thumbnails are visible.
Add scrollbar.
Add option to scale image to window size.