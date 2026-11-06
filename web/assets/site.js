// Gemeinsames Skript beider Seiten (W-03). Jede Funktion prüft, ob ihr Element auf der Seite existiert.
(function(){
  document.documentElement.classList.add('js');
  var reduce = window.matchMedia('(prefers-reduced-motion: reduce)').matches;

  // Mobile-Menü
  var burger=document.getElementById('burger'), menu=document.getElementById('menu');
  if(burger&&menu){burger.addEventListener('click',function(){var o=menu.classList.toggle('open');burger.setAttribute('aria-expanded',o);});
  menu.addEventListener('click',function(e){if(e.target.tagName==='A'){menu.classList.remove('open');burger.setAttribute('aria-expanded','false');}});}

  // Scroll-Fortschritt + aktiver Menüpunkt
  var prog=document.getElementById('progress');
  var links=menu?[].slice.call(menu.querySelectorAll('a[href^="#"]')):[];
  var secs=links.map(function(a){return document.querySelector(a.getAttribute('href'));});
  function onScroll(){
    var h=document.documentElement; var max=h.scrollHeight-h.clientHeight;
    if(prog)prog.style.transform='scaleX('+(max>0?h.scrollTop/max:0)+')';
    var y=h.scrollTop+120, cur=-1;
    secs.forEach(function(s,i){if(s&&s.offsetTop<=y)cur=i;});
    links.forEach(function(a,i){a.classList.toggle('on',i===cur);});
  }
  window.addEventListener('scroll',onScroll,{passive:true}); onScroll();

  // Reveal: nur Elemente unterhalb des Sichtbereichs leicht versetzen, Inhalt bleibt sichtbar
  if(!reduce && 'IntersectionObserver' in window){
    var io=new IntersectionObserver(function(es){es.forEach(function(e){if(e.isIntersecting){e.target.classList.remove('pre');io.unobserve(e.target);}});},{rootMargin:'0px 0px -8% 0px'});
    [].forEach.call(document.querySelectorAll('.rv'),function(el){
      if(el.getBoundingClientRect().top>window.innerHeight){el.classList.add('pre');io.observe(el);}
    });
  }

  // Zähler im Hero
  if(!reduce){
    [].forEach.call(document.querySelectorAll('[data-count]'),function(el){
      var to=+el.dataset.count, pre=el.dataset.prefix||'', suf=el.dataset.suffix||'', t0=null, dur=1400;
      function step(t){if(!t0)t0=t;var p=Math.min(1,(t-t0)/dur);var e=1-Math.pow(1-p,3);el.textContent=pre+Math.round(to*e)+suf;if(p<1)requestAnimationFrame(step);}
      requestAnimationFrame(step);
    });
  }

  // Maus-Licht auf Paketen
  [].forEach.call(document.querySelectorAll('.tier'),function(t){
    t.addEventListener('pointermove',function(e){var r=t.getBoundingClientRect();t.style.setProperty('--mx',(e.clientX-r.left)+'px');t.style.setProperty('--my',(e.clientY-r.top)+'px');});
  });

  // E-Mail kopieren
  var toast=document.getElementById('toast');
  function say(m){toast.textContent=m;toast.classList.add('show');clearTimeout(say.t);say.t=setTimeout(function(){toast.classList.remove('show');},2200);}
  var copyBtn=document.getElementById('copy');
  if(copyBtn)copyBtn.addEventListener('click',function(){
    var txt='lechnerm@ethz.ch';
    function fallback(){var r=document.createRange();r.selectNodeContents(document.getElementById('mail'));var s=getSelection();s.removeAllRanges();s.addRange(r);say('Adresse markiert – jetzt kopieren');}
    if(navigator.clipboard&&navigator.clipboard.writeText){navigator.clipboard.writeText(txt).then(function(){say('E-Mail-Adresse kopiert');},fallback);}else fallback();
  });

  // Binär-Schnee im Hero
  var cv=document.getElementById('snow'); if(!cv||reduce) return;
  var ctx=cv.getContext('2d'), W=0, H=0, dpr=Math.min(2,window.devicePixelRatio||1), parts=[];
  function size(){var r=cv.getBoundingClientRect();W=r.width;H=r.height;cv.width=W*dpr;cv.height=H*dpr;ctx.setTransform(dpr,0,0,dpr,0,0);
    var n=Math.round(Math.min(140,W*H/9000)); parts=[]; for(var i=0;i<n;i++)parts.push(mk(true));}
  function mk(any){var z=Math.random();return{x:Math.random()*W,y:any?Math.random()*H:-20,z:z,v:.25+z*.9,s:9+z*9,c:Math.random()<.5?'0':'1',d:Math.random()<.18,w:Math.random()*6.28};}
  var mx=-999,my=-999; cv.parentElement.addEventListener('pointermove',function(e){var r=cv.getBoundingClientRect();mx=e.clientX-r.left;my=e.clientY-r.top;});
  cv.parentElement.addEventListener('pointerleave',function(){mx=my=-999;});
  var vis=true; new IntersectionObserver(function(e){vis=e[0].isIntersecting;}).observe(cv);
  function frame(){
    if(vis){
      ctx.clearRect(0,0,W,H);
      for(var i=0;i<parts.length;i++){var p=parts[i];
        p.y+=p.v; p.w+=.01; p.x+=Math.sin(p.w)*.25;
        var dx=p.x-mx, dy=p.y-my, dd=dx*dx+dy*dy; if(dd<9000){var f=(9000-dd)/9000*2.2; p.x+=dx/Math.sqrt(dd+1)*f; p.y+=dy/Math.sqrt(dd+1)*f;}
        if(p.y>H+20){parts[i]=mk(false);continue;}
        var a=.12+p.z*.45;
        if(p.d){ctx.strokeStyle='rgba(156,196,242,'+a+')';ctx.lineWidth=1;var r=p.s*.45;ctx.beginPath();for(var k=0;k<3;k++){var an=p.w+k*Math.PI/3;ctx.moveTo(p.x-Math.cos(an)*r,p.y-Math.sin(an)*r);ctx.lineTo(p.x+Math.cos(an)*r,p.y+Math.sin(an)*r);}ctx.stroke();}
        else{ctx.fillStyle='rgba(120,170,235,'+a+')';ctx.font='700 '+p.s+'px "JetBrains Mono", monospace';ctx.fillText(p.c,p.x,p.y);}
      }
    }
    requestAnimationFrame(frame);
  }
  size(); window.addEventListener('resize',size); requestAnimationFrame(frame);
})();
