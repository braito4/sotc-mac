# Publishing the repository

The maintainer can publish this reviewed source tree as a public GitHub repository named
`sotc-mac` under their authenticated account:

```sh
bash packaging/publish-github.sh
```

This requires GitHub CLI access from a Terminal with network connectivity. If needed,
authenticate first with `gh auth login -h github.com`. Do not paste access tokens into chat,
issues or files in this repository.

The script checks the source index and requires committed changes. It preserves an unrelated
existing repository or unexpected remote. It never force-pushes. Retrying a partial creation
can push to the matching existing origin without replacing its history.

Only committed sources and pinned upstream submodule pointers are pushed. Game inputs,
private generated images, execution logs, ZIP archives and local state remain ignored. The
script does not create a binary release or claim that the game is playable.
