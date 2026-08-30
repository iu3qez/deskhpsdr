# CLAUDE.md — deskhpsdr

## Topologia del repo

Questo è un fork. Due remote, ruoli distinti:

| remote | URL | ruolo |
|---|---|---|
| `origin` | `iu3qez/deskhpsdr` | il nostro fork — qui si pusha |
| `upstream` | `dl1bz/deskhpsdr` | progetto originale — da qui si fetcha, mai push |

Branch: `master` segue upstream; `windows`, `windows-client-server`,
`windows-porting` sono lavoro nostro di porting Windows che **non esiste in
upstream**. Non rebasare né force-pushare quelle tre senza chiedere.

## PR verso upstream

**`.gitignore` va sempre escluso dalle PR verso `dl1bz/deskhpsdr`.**

È un file tracciato da upstream che noi modifichiamo per esigenze locali
(scratch di tooling, artefatti di build nostri). Includerlo in una PR
significa proporre a upstream regole che riguardano solo il nostro workflow,
e genera conflitti ricorrenti a ogni merge da `upstream/master`.

Prima di aprire una PR verso upstream, verificare e rimuovere:

```sh
git diff upstream/master --stat -- .gitignore   # deve essere vuoto nella PR
git restore --source=upstream/master .gitignore # se serve ripulire il branch di PR
```

Stessa regola per qualunque altro file di configurazione locale che finisca
per divergere da upstream: la PR deve contenere solo la modifica funzionale.

## Compound Engineering

Artefatti CE (plans, solutions, ideation, explainers) in `ce_docs/`,
configurato via `docs_root` in `.compound-engineering/config.yaml`.
Anche `ce_docs/` è roba nostra: fuori dalle PR verso upstream.

## Build

Progetto C con GTK. Vedi `Makefile` e `MacOS/` per i target macOS.
