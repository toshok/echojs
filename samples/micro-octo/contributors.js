// ../../ejs --moduledir ../../node-compat/ -I micro-octo=. contributors.js 

import { GithubClient } from '$micro-octo/micro-octo';

let client = new GithubClient('AvocadoInc');

client.Repository.getAllContributors('toshok', 'echojs')
    .then(contributors => {
        console.log('Contributors');
        contributors.forEach(contributor => console.log(` ${contributor}`));
    });

