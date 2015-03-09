// ../../ejs --moduledir ../../node-compat/ -I micro-octo=. user.js

import { GithubClient } from '$micro-octo/micro-octo';

let client = new GithubClient('AvocadoInc');

client.User.get('carlosalberto')
    .then(user => {
        console.log(`User: ${user}`);

        client.Organization.getAll(user.login)
            .then(orgs => {
                console.log('Organizations:');
                orgs.slice(0, 5).forEach(org => console.log(` ${org}`));
            });

        client.Repository.getAllForUser(user.login)
            .then(repos => {
                console.log('Repositories:');
                repos.slice(0, 5).forEach(repo => console.log(` ${repo}`));
            });

        client.Gist.getAllForUser(user.login)
            .then(gists => {
                console.log('Gists:');
                gists.slice(0, 5).forEach(gist => console.log(` ${gist}`));
            });

    }).catch(error => console.log(`Error: ${error}`));

