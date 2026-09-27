// Test the actual page's command/error logic without a browser or a device.
const fs=require('node:fs'),vm=require('node:vm'),assert=require('node:assert/strict'),path=require('node:path');
class Element {
 constructor(){this.value='';this.textContent='';this.options=[];this.style={};this.dataset={};this.classList={toggle(){},add(){},remove(){}};}
 querySelector(){return this.child||(this.child=new Element());}
 replaceChildren(){this.options=[];}
 add(option){this.options.push(option);if(!this.value)this.value=option.value;}
}
const elements=new Map();const el=id=>{if(!elements.has(id))elements.set(id,new Element());return elements.get(id);};
const html=fs.readFileSync(path.join(__dirname,'../../package/main.html'),'utf8');
const script=html.match(/<script>([\s\S]*?)<\/script>/)[1];
new vm.Script(script);
const state={ok:true,language:'en',enabled:true,auto_connect_supported:true,started:true,ready:true,connected:true,
 phase:'ready',config:{auto_connect:true},raw:{},output:{buttons:0},last_error:'',command_id:0,command_status:'none'};
let response;
const context=vm.createContext({document:{getElementById:el,querySelectorAll:()=>[],documentElement:{},title:''},
 location:{pathname:'/hidpad/'},localStorage:{getItem:()=>null},Option:class{constructor(text,value){this.textContent=text;this.value=value;}},
 fetch:async()=>({ok:true,json:async()=>response}),setTimeout:()=>0,clearTimeout(){},console});
vm.runInContext(script.slice(0,script.indexOf("document.addEventListener('click'"))+`
 globalThis.test={paintState,paintInput,command,busy:()=>busy,pending:()=>pendingCommand};`,context);
const test=context.test;
(async()=>{
 test.paintState({...state,ready:false,connected:false,phase:'error',last_error:'Pairing failed'});
 assert.equal(el('message').hidden,false);
 test.paintInput(state);
 assert.equal(el('message').hidden,true,'successful reconnection clears old error');
 response={ok:true,command_id:42,state:{...state,command_id:42,command_kind:'disconnect',command_status:'pending'}};
 await test.command('disconnect');
 assert.equal(test.busy(),true);assert.equal(test.pending(),42);
 assert.equal(el('message').textContent,'Working…','queue acceptance must not show success');
 test.paintInput({...state,ready:false,connected:false,command_id:42,command_status:'failed',command_error:'disconnect failed',last_error:'disconnect failed'});
 assert.equal(test.busy(),false);assert.match(el('message').textContent,/disconnect failed/);
 test.paintInput({...state,command_id:42,command_status:'failed',command_error:'disconnect failed'});
 assert.equal(el('message').hidden,true,'historic failed command does not reappear after recovery');
 console.log('Web: async completion and recovered-error checks passed');
})().catch(error=>{console.error(error);process.exitCode=1;});
