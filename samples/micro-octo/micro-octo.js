// ../../ejs.exe --moduledir ../../node-compat/ micro-octo.js

import * as path from '@node-compat/path';
import * as os from '@node-compat/os';

const Version = '0.0.1';
const RootUrl = 'api.github.com';

class User {
    constructor(login, name, email, company, bio, followers, following) {
        this.login = login;
        this.name = name;
        this.email = email;
        this.company = company;
        this.bio = bio;
        this.followers = followers;
        this.following = following;
    }

    static fromJSON(json) {
        return new User(json.login, json.name, json.email, json.company, json.bio, json.followers, json.following);
    }

    toString() {
        return `${this.name}, login: ${this.login}, email: ${this.email}`;
    }
}

class Users {
    constructor(connection) {
        this.connection = connection;
    }

    get(login) {
        return this.connection.get(path.join('users', login))
            .then(user => User.fromJSON(user));
    }
}

class Organization {
    constructor(login, name, description) {
        this.login = login;
        this.name = name;
        this.description = description;
    }

    static fromJSON(json) {
        return new Organization(json.login, json.name, json.description);
    }

    toString() {
        return `${this.login}, name: ${this.name}, desc: ${this.description}`;
    }
}

class Organizations {
    constructor(connection) {
        this.connection = connection;
    }

    get(orgname) {
        return this.connection.get(path.join('orgs', orgname))
            .then(org => Organization.fromJSON(org));
    }

    getAll(login) {
        let url = path.join('users', login, 'orgs');
        return this.connection.get(url)
            .then(orgs => orgs.map(org => Organization.fromJSON(org)));
    }
}

class Repository {
    constructor(name, owner, description, language) {
        this.name = name;
        this.owner = owner;
        this.description = description;
        this.language = language;
    }

    static fromJSON(json) {
        return new Repository(json.name, json.owner.login, json.description, json.language);
    }

    toString() {
        return `${this.name}, owner: ${this.owner}, language: ${this.language}`;
    }
}

class RepositoryContributor {
    constructor(login, contributions) {
        this.login = login;
        this.contributions = contributions;
    }

    static fromJSON(json) {
        return new RepositoryContributor(json.login, json.contributions);
    }

    toString() {
        return `${this.login}, contributions: ${this.contributions}`;
    }
}

class Repositories {
    constructor(connection) {
        this.connection = connection;
    }

    get(login, name) {
        return this.connection.get(path.join('repos', login, name))
            .then(repo => Repository.fromJSON(repo));
    }

    getAllContributors(login, name) {
        return this.connection.get(path.join('repos', login, name, 'contributors'))
            .then(contributors => contributors.map(contributor => RepositoryContributor.fromJSON(contributor)));
    }

    getAllForUser(login) {
        return this.connection.get(path.join('users', login, 'repos'))
            .then(repos => repos.map(repo => Repository.fromJSON(repo)));
    }

    getAllForOrg(login) {
        return this.connection.get(path.join('orgs', login, 'repos'))
            .then(repos => repos.map(repo => Repository.fromJSON(repo)));
    }
}

class Gist {
    constructor(id, description, isPublic, files) {
        this.id = id;
        this.description = description;
        this.isPublic = isPublic;
        this.files = files;
    }

    static fromJSON(json) {
        /* Replace the files with a clean, short version */
        let files = [];
        for (let filename in json.files)
            files.push(GistFile.fromJSON(json.files[filename]));

        return new Gist(json.id, json.description, json.public, files);
    }

    toString() {
        return `${this.id}, desc: ${this.description}, files: ${this.files}`;
    }
}

class GistFile {
    constructor(filename, content) {
        this.filename = filename;
        this.content = content;
    }

    static fromJSON(json) {
        return new GistFile(json.filename, json.content);
    }

    toString() {
        return `${this.filename}`;
    }
}

class Gists {
    constructor(connection) {
        this.connection = connection;
    }

    get(id) {
        return this.connection.get(path.join('gists', id))
            .then(gist => Gist.fromJSON(gist));
    }

    getAllForUser(login) {
        return this.connection.get(path.join('users', login, 'gists'))
            .then(gists => gists.map(gist => Gist.fromJSON(gist)));
    }
}

class RateLimit {
    constructor(core, search) {
        this.core = core;
        this.search = search;
    }

    static fromJSON(json) {
        return new RateLimit(json.resources.core, json.resources.search);
    }

    toString() {
        return `limit: ${this.core.limit}, remaining: ${this.core.remaining}`;
    }
}

class Connection {
    constructor(productName) {
        this.userAgent = this.formatUserAgent(productName);
    }

    formatUserAgent(productName) {
        let platform = os.platform();
        let release = os.release();
        let arch = os.arch();
        let culture = 'us'; // 'us' for now.

        return `${productName} (${platform} ${release}; ${arch}; ${culture}; MicroOcto ${Version})`;
    }

    createRequestObj(method, requestPath) {
        let request = new XMLHttpRequest();
        request.open(method, 'https://' + path.join(RootUrl, requestPath));

        request.setRequestHeader('User-Agent', this.userAgent);
        request.setRequestHeader('Content-Type', 'application/json');
        request.setRequestHeader('Accept', 'application/vnd.github.v3+json');

        return request;
    }

    get(path) {
        let request = this.createRequestObj('GET', path);
        return new Promise((resolve, reject) => {

            request.onreadystatechange = () => {
                if (request.readyState != 4)
                    reject(Error('Not possible to complete request, internal status code : ' + request.readyState));

                if (request.status < 200 || request.status >= 300)
                    reject(Error('Failed to complete request, code: ' + request.status));
                else
                    resolve(JSON.parse(request.responseText), request.responseText);

            };

            request.send();
        });
    }
}

export class GithubClient {
    constructor(clientName) {
        let connection = new Connection(clientName);

        this.connection = connection;
        this.Gist = new Gists(connection);
        this.Organization = new Organizations(connection);
        this.Repository = new Repositories(connection);
        this.User = new Users(connection);
    }

    getRateLimit() {
        return this.connection.get('rate_limit')
            .then(value => RateLimit.fromJSON(value));
    }
}

/*
*/

