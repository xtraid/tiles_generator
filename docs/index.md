---
layout: default
title: Tiling Foundry
page_kind: home
description: A research software laboratory for finite Wang tilings and inspectable solver design.
---

<section class="home-hero" data-wang-sections data-content-column markdown="1">
  <p class="eyebrow">Finite tilings / algorithm laboratory</p>

  # Tiling Foundry

  Tiling Foundry turns a mathematical reduction into an inspectable software
  pipeline. It keeps construction, solving, verification, and measurement
  separate so that each result can be audited rather than merely observed.

  [Explore the source]({{ site.repository_url }}){: .text-link }
</section>

<section class="home-notes" id="notes" data-content-column markdown="1">
  <div class="section-heading">
    <p class="eyebrow">Field notes</p>
    <h2>Research log</h2>
  </div>

  {% include post-list.html %}
</section>

<section class="home-index" data-content-column markdown="1">
  <div class="section-heading">
    <p class="eyebrow">Working record</p>
    <h2>Technical archive</h2>
  </div>

  The repository also publishes its implementation contracts and measured
  solver notes as first-class project documents.

  - [Development principles]({{ '/development_principles/' | relative_url }})
  - [Yang–Zhang region builder]({{ '/yang_zhang_builder_design/' | relative_url }})
  - [Serial solver implementation guide]({{ '/serial_solver_implementation_guide/' | relative_url }})
  - [Solver performance scope]({{ '/solver_performance_scope/' | relative_url }})
  - [Reference profile]({{ '/solver_reference_profile_2026-08-17/' | relative_url }})
  - [Project references]({{ '/references/' | relative_url }})
</section>
