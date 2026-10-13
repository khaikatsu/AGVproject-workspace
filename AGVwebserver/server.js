// Entrypoint giữ nguyên: node server.js hoặc npm start.
const {createGateway} = require('./gateway');
if (require.main === module) {
    const gateway = createGateway();
    gateway.start().then(address => console.log(`[HTTP] Dashboard: ${process.env.AGV_HTTPS_CERT ? 'https' : 'http'}://localhost:${address.port}`))
        .catch(async error => {
            console.error(error.code === 'EADDRINUSE'
                ? `Cổng ${error.port || process.env.PORT || 3000} đang được sử dụng. Nếu gateway đã chạy, mở web hiện có; muốn chạy lại thì dừng đúng phiên cũ trước. Mô phỏng dùng scripts/run-simulation.ps1 và cổng 3100.`
                : error);
            process.exitCode = 1;await gateway.close();
        });
    for (const signal of ['SIGINT','SIGTERM']) process.once(signal, () => gateway.close().then(() => process.exit()));
}
module.exports = {createGateway};
