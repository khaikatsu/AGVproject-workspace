$ErrorActionPreference = 'Stop'

$ruleName = 'AGV MQTT hotspot TCP 1883'
$existingRule = Get-NetFirewallRule -DisplayName $ruleName -ErrorAction SilentlyContinue
if ($existingRule) {
    $port = $existingRule | Get-NetFirewallPortFilter
    $address = $existingRule | Get-NetFirewallAddressFilter
    if ($existingRule.Enabled -ne 'True' -or $existingRule.Direction -ne 'Inbound' -or
        $existingRule.Action -ne 'Allow' -or $existingRule.Profile -ne 'Public' -or
        $port.Protocol -ne 'TCP' -or $port.LocalPort -ne '1883' -or
        $address.LocalAddress -ne '192.168.137.1' -or
        $address.RemoteAddress -ne '192.168.137.0/24') {
        throw 'Existing rule has a different scope; inspect it manually.'
    }
    exit 0
}

New-NetFirewallRule -DisplayName $ruleName -Direction Inbound -Action Allow `
    -Protocol TCP -LocalPort 1883 -LocalAddress 192.168.137.1 `
    -RemoteAddress 192.168.137.0/24 -Profile Public -PolicyStore PersistentStore | Out-Null
