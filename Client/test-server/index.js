const express = require('express'),
    app = express(),
    server = require('http').createServer(app);

app.use(express.json());
app.use(express.urlencoded({ extended: true }));

app.get("/", (req, res) => {
    res.send("test?");
});

app.post("/", (req, res) => {
    console.log('body: ' + JSON.stringify(req.body));

    let response = { received: JSON.stringify(req.body) };
    res.send(JSON.stringify(response));
})

server.listen(3000, () => {
    console.log("Server listening on port 3000");
});
