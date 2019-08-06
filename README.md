# QuickMedia
Native clients of websites with fast access to what you want to see.
[Demo with manga](https://beta.lbry.tv/quickmedia_manga-2019-08-05_21.20.46/7)
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