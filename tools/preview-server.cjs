// Local design preview only. No BLE/device access and no configuration writes.
const http = require('node:http');
const fs = require('node:fs');
const path = require('node:path');
const names = ['UP','DOWN','LEFT','RIGHT','A','B','X','Y','L','R','LS','RS','SELECT','START','SHARE','HOME'];
const defaults = () => ({auto_connect:true,deadzone:3200,mapping:Object.fromEntries(names.map(n=>[n,n]))});
const devices = [{name:'Xbox Wireless Controller',address:'A4:C1:38:92:7B:06',rssi:-48},{name:'GamepadSpace-Q34U',address:'D2:64:11:8F:03:AB',rssi:-63}];
let config=defaults(),enabled=true,connected=true,calibrating=false,name=devices[0].name,address=devices[0].address;
const state = language => ({ok:true,version:'preview',language,enabled,auto_connect_supported:true,connected,ready:connected,started:enabled,scanning:false,command_id:0,command_status:"none",connecting:false,phase:enabled?(connected?'ready':'select_device'):'stopped',name:connected?name:'',address:connected?address:'',raw:{lx:0,ly:0,rx:0,ry:0,lt:0,rt:0,buttons:0},output:{buttons:0},config,calibrating});
const server=http.createServer(async(req,res)=>{
 const url=new URL(req.url,'http://127.0.0.1');
 const ref=new URL(req.headers.referer||url.href);
 const language=ref.searchParams.get('lang')||url.searchParams.get('lang')||'zh-CN';
 res.setHeader('Cache-Control','no-store');
 if(url.pathname==='/hidpad/'||url.pathname==='/hidpad'||url.pathname==='/'){
  let page=fs.readFileSync(path.join(__dirname,'../package/main.html'),'utf8');
  const banner='<div class="preview-banner">本地预览 · 模拟数据</div>';
  res.setHeader('Content-Type','text/html; charset=utf-8');res.end(page.replace('<body>','<body>'+banner));return;
 }
 res.setHeader('Content-Type','application/json; charset=utf-8');
 let result;
 if(url.pathname.endsWith('/api/state'))result=state(language);
 else if(url.pathname.endsWith('/api/input')){const s=state(language);result={...s,...s.raw,events:0};}
 else if(url.pathname.endsWith('/api/devices'))result={ok:true,devices,scanning:false};
 else if(url.pathname.endsWith('/api/command')&&req.method==='POST'){
  let body='';for await(const chunk of req){body+=chunk;if(body.length>4096){res.statusCode=413;res.end('{}');return;}}
  try{
   const {topic,payload={}}=JSON.parse(body);
   if(topic==='set_config')Object.assign(config,payload);
   if(topic==='set_mapping')config.mapping=payload.mapping;
   if(topic==='restore_defaults'){config=defaults();calibrating=false;}
   if(topic==='disable'){enabled=false;connected=false;}
   if(topic==='enable')enabled=true;
   if(topic==='disconnect'||topic==='forget')connected=false;
   if(topic==='connect_device'){const d=devices.find(d=>d.address===payload.address);if(d){connected=true;name=d.name;address=d.address;}}
   if(topic==='calibration_start')calibrating=true;
   if(topic==='calibration_save'||topic==='calibration_cancel')calibrating=false;
   result={ok:true,state:state(language)};
  }catch{res.statusCode=400;result={ok:false,error:'invalid request'};}
 }else{res.statusCode=404;result={ok:false};}
 res.end(JSON.stringify(result));
});
server.listen(Number(process.env.HIDPAD_PREVIEW_PORT)||8766,'127.0.0.1',()=>console.log('HID Pad preview: http://127.0.0.1:8766/hidpad/'));
