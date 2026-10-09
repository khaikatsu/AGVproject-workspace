// Entrypoint giữ nguyên: node server.js hoặc npm start.
const {createGateway} = require('./gateway');
if (require.main === module) {
    const gateway = createGateway();
    gateway.start().then(address => console.log(`[HTTP] Dashboard: ${process.env.AGV_HTTPS_CERT ? 'https' : 'http'}://localhost:${address.port}`))
        .catch(error => {console.error(error);process.exitCode = 1;gateway.close();});
    for (const signal of ['SIGINT','SIGTERM']) process.once(signal, () => gateway.close().then(() => process.exit()));
}
module.exports = {createGateway};
