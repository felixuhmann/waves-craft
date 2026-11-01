#include <math.h>
#include <stdlib.h>
#define NR 8
static const double NF[NR]={0.5,2,5,9,14,19,24,30};
typedef struct{double taur[NR],tau0,F1,p1,e0,p2,kmax,Smax,F0,a,b,m,taus;}P;
static double taur(const P*p,double F){ if(F<=NF[0])return p->taur[0]; for(int i=1;i<NR;i++) if(F<NF[i]){double u=(F-NF[i-1])/(NF[i]-NF[i-1]);return p->taur[i-1]+u*(p->taur[i]-p->taur[i-1]);} return p->taur[NR-1];}
static double sinf_(const P*p,double F){return p->Smax*(1-exp(-F/p->F0));}
static double solveT(const P*p,double G){ if(G<=0)return 0; double lo=0,hi=G; for(int i=0;i<40;i++){double m=.5*(lo+hi); if(m+sinf_(p,m)<G)lo=m;else hi=m;} return .5*(lo+hi);}
void simulate(const double*pv,double G,double bms,double total_ms,double dt,double*g,double*Fo,double*So,int n){
 const P*p=(const P*)pv; double T=solveT(p,G),F=0,S=0;
 for(int i=0;i<n;i++){ double t=i*dt; double Tt=t<bms?T:0;
  double e=Tt-F,dF;
  if(e>0){ double tau=p->tau0*(1+pow(F/p->F1,p->p1))*(1+pow(p->e0/(e+1e-6),p->p2)); double r=e/tau; dF=r/(1+r/p->kmax);}
  else { double rF=F/taur(p,F), rT=Tt>0?Tt/taur(p,Tt):0; dF=-(rF-rT); if(dF>0)dF=0; }
  double Si=sinf_(p,F),dS;
  if(S<Si){ double q=1-S/Si; dS=Si*p->a*pow(F+1e-9,p->b)*pow(q,p->m); double lim=(Si-S)/dt; if(dS>lim)dS=lim;} else dS=(Si-S)/p->taus;
  F+=dt*dF; if(F<0)F=0; S+=dt*dS; if(S<0)S=0;
  g[i]=F+S; if(Fo){Fo[i]=F;So[i]=S;}
 }
}
