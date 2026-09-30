/* Development-only independent fixtures. Install the pinned reference under an
 * ignored directory; no Minecraft bytecode/assets are included in the output.
 * Usage: node tools/generate-physics-reference.cjs /absolute/reference/package
 */
const fs = require('node:fs')
const path = require('node:path')
const { createRequire } = require('node:module')
const base = path.resolve(process.argv[2])
const fromReference = createRequire(path.join(base, 'index.js'))
const { Physics, PlayerState } = fromReference(base)
const { Vec3 } = fromReference('vec3')
const data = fromReference('minecraft-data')('1.21.1')
const Block = fromReference('prismarine-block')('1.21.1')
const world = { getBlock(pos) {
  const block = new Block(pos.y < 0 ? data.blocksByName.stone.id : data.blocksByName.air.id, 0, 0)
  block.position = pos; return block
} }
const physics = Physics(data, world)
const scenarios = [
  ['walk', 0, 0, t => ({ forward: t < 45 })],
  ['sprint', 0, 0, t => ({ forward: t < 45, sprint: true })],
  ['sneak', 0, 0, t => ({ forward: t < 45, sneak: true })],
  ['diagonal', 0, 0, t => ({ forward: t < 45, right: t < 45 })],
  ['jump', 0, 0, t => ({ jump: t === 0 })],
  ['held_jump', 0, 0, t => ({ jump: t < 45 })],
  ['sprint_jump', 0, 0, t => ({ forward: true, sprint: true, jump: t === 20 })],
  ['fall', 0, 1000, t => ({})],
  ['air_steer', 0, 1000, t => ({ forward: t < 30, right: t >= 30 })],
  ['flight', 1, 1000, t => ({ forward: t < 45 })],
  ['flight_sprint', 1, 1000, t => ({ forward: true, sprint: t >= 25 })],
  ['flight_vertical', 1, 1000, t => ({ jump: t < 20, sneak: t >= 40 })],
  ['glide', 2, 1000, t => ({})],
  ['glide_dive_climb', 2, 1000, t => ({ pitch: t < 30 ? .7 : -.5 })],
  ['glide_boost', 2, 1000, t => ({ pitch: -.15, boost: t < 30 })]
]
let output = 'scenario,tick,mode,grounded,forward,strafe,jump,descend,sprint,boost,pitch,dx,dy,dz\n'
for (const [name, mode, altitude, controlsAt] of scenarios) {
  const controls = {}
  const bot = { entity: { position: new Vec3(0, altitude, 0), velocity: new Vec3(0,0,0),
    onGround: altitude === 0, flying: mode === 1, flyingSpeed: .05,
    elytraFlying: mode === 2, yaw: Math.PI, pitch: 0, effects: {} },
    jumpTicks: 0, jumpQueued: false, fireworkRocketDuration: 0, version: '1.21.1', inventory: { slots: [] } }
  const state = new PlayerState(bot, controls)
  state.elytraEquipped = true
  for (let tick = 0; tick < 60; ++tick) {
    for (const key of ['forward','back','left','right','jump','sneak','sprint']) controls[key] = false
    const current = controlsAt(tick); Object.assign(controls, current)
    state.pitch = -(current.pitch || 0)
    state.fireworkRocketDuration = current.boost ? 1 : 0
    const old = state.pos.clone(), ground = state.onGround
    physics.simulatePlayer(state, world)
    const delta = state.pos.minus(old)
    output += [name,tick,mode,+ground,+!!controls.forward,+!!controls.right,+!!controls.jump,+!!controls.sneak,
      +!!controls.sprint,+!!current.boost,current.pitch || 0,-delta.x,delta.z,delta.y].join(',')+'\n'
  }
}
fs.mkdirSync('tests/fixtures', { recursive: true })
fs.writeFileSync('tests/fixtures/minecraft-java-1.21.1.csv', output)
console.log('Wrote', scenarios.length * 60, 'independent reference ticks')
