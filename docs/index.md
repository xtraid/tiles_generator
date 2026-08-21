---
layout: default
title: Tiling Foundry
page_kind: home
description: A research software laboratory for finite Wang tilings and inspectable solver design.
---

<section class="home-hero" data-wang-sections data-content-column markdown="1">
  <p class="eyebrow">Finite tilings / algorithm laboratory</p>

  # Tiling Foundry

  Tiling Foundry turns the Yang–Zhang reduction into an inspectable software
  pipeline. Construction, solving, verification, witness correspondence, and
  measurement remain separate so that each result can be audited rather than
  merely observed.

  [Explore the documentation](#documentation){: .text-link }
</section>

{% assign architecture = site.pages | where: "section", "Architecture and correctness" | sort: "nav_order" %}
{% assign reduction = site.pages | where: "section", "Yang–Zhang reduction" | sort: "nav_order" %}
{% assign optimization = site.pages | where: "section", "Solver optimization" | sort: "nav_order" %}
{% assign comparisons = site.pages | where: "section", "Cross-engine benchmarks" | sort: "nav_order" %}
{% assign historical = site.pages | where: "section", "Historical material" | sort: "nav_order" %}

<section class="home-index" id="documentation" data-content-column markdown="1">
  <div class="section-heading">
    <p class="eyebrow">Start here</p>
    <h2>Architecture and correctness</h2>
  </div>

  These documents explain the software boundaries that keep the reduction,
  solver, independent verification, and Boolean–Wang witness correspondence
  auditable.

  {% include document-list.html documents=architecture %}
</section>

<section class="home-index" id="yang-zhang-reduction" data-content-column markdown="1">
  <div class="section-heading">
    <p class="eyebrow">Construction</p>
    <h2>Yang–Zhang reduction</h2>
  </div>

  Read these pages for the mathematical conventions, region geometry,
  implementation contract, and primary sources behind the fixed 23-tile
  construction.

  {% include document-list.html documents=reduction %}
</section>

<section class="home-index" id="solver-optimization" data-content-column markdown="1">
  <div class="section-heading">
    <p class="eyebrow">Measured mechanisms</p>
    <h2>Solver optimization</h2>
  </div>

  The optimization record starts from the retained reference path and adds one
  measured mechanism at a time. Each report preserves its corpus, environment,
  direct work counters, timing method, and limitations.

  {% include document-list.html documents=optimization %}
</section>

<section class="home-index" id="cross-engine-benchmarks" data-content-column markdown="1">
  <div class="section-heading">
    <p class="eyebrow">Native C / Z3</p>
    <h2>Cross-engine benchmarks</h2>
  </div>

  The comparison protocol distinguishes solving the same prepared Wang region
  from end-to-end decisions that begin with the same formula file.

  {% include document-list.html documents=comparisons %}
</section>

<section class="home-index" id="historical-material" data-content-column markdown="1">
  <div class="section-heading">
    <p class="eyebrow">Design history</p>
    <h2>Historical material</h2>
  </div>

  Earlier proposals are retained to explain the project’s design trajectory.
  They are not current API contracts.

  {% include document-list.html documents=historical %}
</section>
