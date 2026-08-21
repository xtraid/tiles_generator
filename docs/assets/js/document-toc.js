const article = document.querySelector(".article__body");
const navigation = document.querySelector("[data-document-toc]");

if (article && navigation) {
  const headings = [...article.querySelectorAll("h2")];

  if (headings.length >= 2) {
    const list = navigation.querySelector("ol");

    for (const heading of headings) {
      if (!heading.id) {
        continue;
      }

      const item = document.createElement("li");
      const link = document.createElement("a");
      link.href = `#${encodeURIComponent(heading.id)}`;
      link.textContent = heading.textContent;
      item.append(link);
      list.append(item);
    }

    if (list.children.length >= 2) {
      navigation.hidden = false;
    }
  }
}
