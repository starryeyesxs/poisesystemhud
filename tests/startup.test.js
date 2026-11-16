const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const test = require('node:test');

const source = fs.readFileSync(path.join(__dirname, '..', 'src', 'main.cpp'), 'utf8').replace(/\r\n/g, '\n');

function section(start, end) {
    const begin = source.indexOf(start);
    const finish = source.indexOf(end, begin + start.length);
    assert.ok(begin >= 0 && finish > begin, `Missing source section ${start}`);
    return source.slice(begin, finish);
}

test('view and timer start and the only key callback is F10', () => {
    const dataLoaded = section('case SKSE::MessagingInterface::kDataLoaded:', 'case SKSE::MessagingInterface::kPreLoadGame:');
    assert.ok(dataLoaded.indexOf('CreateHudView();') < dataLoaded.indexOf('StartPolling();'));
    assert.ok(dataLoaded.indexOf('StartPolling();') < dataLoaded.indexOf('ui->AddEventSink(&menuVisibilitySink)'));
    assert.ok(!dataLoaded.includes('inventoryChangeSink'));
    assert.ok(!dataLoaded.includes('hotkeyScanCode'));
    assert.ok(!dataLoaded.includes('highlightHotkey'));
    const registerKey = section('void RegisterReloadKey()\n\t{', 'class MenuVisibilitySink');
    assert.match(registerKey, /scanCode\s*=\s*0x44/);
    assert.equal((source.match(/KeyHandler::GetSingleton\(\)->Register\(/g) || []).length, 1);
    assert.ok(source.includes('Before PrismaUI->CreateView'));
    assert.ok(source.includes('After PrismaUI->CreateView'));
});

test('HUD visibility and vanilla meters do not depend on hotbar initialization', () => {
    const poll = section('void PollHud()', 'VOID CALLBACK TimerCallback');
    assert.match(poll, /HideVanillaMeters\(\)/);
    assert.match(poll, /UpdateHudVisibility\(\)/);
    assert.doesNotMatch(poll, /hotbar/i);
    const visibility = section('void UpdateHudVisibility()', 'void SendPlayerStats()');
    assert.ok(!visibility.includes('hotbarDirty'));
    const create = section('void CreateHudView()', 'void PollHud()');
    assert.ok(!create.includes('IsGameplayReady()'));
});

test('passive always-open overlays do not block the HUD', () => {
    const blockers = section('bool HasBlockingMenus()', 'void UpdateHudVisibility()');
    assert.match(blockers, /ui->GetMenu\(name\)/);
    assert.match(blockers, /HudMenuVisibility::IsPassiveOverlay\(menu->AlwaysOpen\(\), menu->PausesGame\(\)/);
    assert.match(blockers, /menu->UsesMenuContext\(\), menu->UsesCursor\(\), menu->ApplicationMenu\(\)/);
    assert.match(blockers, /HUD hidden by open menus/);
    assert.match(blockers, /HUD ignoring passive overlay menus/);
    const visibility = section('void UpdateHudVisibility()', 'void SendPlayerStats()');
    assert.match(visibility, /!HasBlockingMenus\(\)/);
});
