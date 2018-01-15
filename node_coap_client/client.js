var coap = require('coap');

/**
 * Funcionamiento
 * instalar coap: npm install coap --save
 * preder led: node client.js ledh
 * preder apagar: node client.js ledl
 */

var req = coap.request('coap://192.168.10.6/' + process.argv[2]);

req.on('response', function(res) {
    res.pipe(process.stdout);

    res.on('end', function() {
        process.exit(0);
    })
})

req.end();
