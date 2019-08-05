# QuickMedia
Native clients of websites with fast access to what you want to see
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
Keep track of content that has been viewed so the user can return to where they were last.
For manga, view the next chapter when reaching the end of a chapter.
Make network requests asynchronous to not freeze gui when navigating. Also have loading animation.