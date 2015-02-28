function fetchUrl (url) {
  let promise = new Promise((resolve, reject) => {
    let client = new XMLHttpRequest();
    client.open("GET", url);
    client.onreadystatechange = () => {
      if (client.readyState !== 4)
        return;
      if (client.status === 200)
        resolve(client.responseText);
      else
        reject(client);
    };
    client.send();
  });

  return promise;
}

if (process.argv.length < 2) {
  console.log(`usage:  ${process.argv[0]} $url`);
  process.exit(-1);
}
let url_to_fetch = process.argv[1];

console.time("fetch url");
fetchUrl(url_to_fetch).then((data) => {
  console.log(data);
  console.timeEnd("fetch url");
}).catch((err) => {
  console.log(`error fetching ${url_to_fetch}`);
  console.log(err);
  console.timeEnd("fetch url");
});
