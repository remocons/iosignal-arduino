// io-server on same computer
var io = new IO('ws://localhost:7777')   

// LAN ip address.
// var io = new IO('ws://192.168.0.204:7777')  

// public io-server secure connection.
// var io = new IO('wss://io.remocon.kr/ws')  

serverURL.innerText = io.url
let channelTag = '#homeButton'

function sendSignal(){
  io.signal( channelTag, "browser")
}


io.on('error', (e)=>{
  console.log('err',e)
} )    

io.on('change', ()=>{
  console.log('state:', io.stateName , io.state)
  ioState.innerText = io.state; // Number
  ioStateName.innerText = io.stateName; // String
})

// Subscribe to a channel and register a channel message handler.
io.on('ready',()=>{
  io.subscribe(channelTag)
  io.on(channelTag, onMessage )
})

// The listen function allows you to subscribe and register handlers at once.
// *The listen function is not supported on Arduino.
// io.listen( channelTag, onMessage )

function onMessage(...args){
  let msg = JSON.stringify( args )
  let p = document.createElement('article')
  p.textContent = "🕑 "+ String( Date.now() - beginAt) + "ms  " + msg
  let v = document.querySelector('#messageView')
  if(v.childElementCount > 3){
    v.removeChild( v.lastElementChild )
  } 
  v.prepend( p )
  toggleLightBulb();
}

let beginAt = Date.now()


function toggleLightBulb(){
  let buttonState = light_bulb.style.fill
  if(buttonState == "gray" ){
    light_bulb.style.fill = "rgb(255,200,0)"
  }else{
    light_bulb.style.fill = "gray"
  }
}