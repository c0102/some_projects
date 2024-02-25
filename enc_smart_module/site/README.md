# Built-in website with svelte

The steps needed to build the website that lives on the chip

## node.js

First you need node,js on your pc: 
[download node.js](https://nodejs.org/en/download/)

## Install svelte, Tailwind CSS and Daisy UI 

You need to be in the ```site``` directory for the following commands. TO install svelte you run 

```bash
npm install
```
Then we install Tailwind CSS 

```bash
npm install -D tailwindcss postcss autoprefixer svelte-preprocess
```
And finally we install Daisy UI 

```bash
npm i daisyui
```
## Run and Build the site

To run a local preview of the site use:

```bash
npm run dev
```
You can then follow the given instructions printed on the terminal to access the site from a browser, usually an IP address that begins with 192.168....

### Building the site
To build the site run:

```bash
npm run build
```
If everything has gone correctly you should see a new directory called ```build``` appear inside the ```site``` directory

