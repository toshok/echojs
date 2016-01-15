import * as ejsfs from   '@node-compat/fs';
import * as ejspath from '@node-compat/path';
import * as uikit from   '$pirouette/uikit';
// glMatrix v0.9.5
let glMatrixArrayType=Float32Array;var vec3={};vec3.create=function(a){var b=new glMatrixArrayType(3);if(a){b[0]=a[0];b[1]=a[1];b[2]=a[2]}return b};vec3.set=function(a,b){b[0]=a[0];b[1]=a[1];b[2]=a[2];return b};vec3.add=function(a,b,c){if(!c||a==c){a[0]+=b[0];a[1]+=b[1];a[2]+=b[2];return a}c[0]=a[0]+b[0];c[1]=a[1]+b[1];c[2]=a[2]+b[2];return c};
vec3.subtract=function(a,b,c){if(!c||a==c){a[0]-=b[0];a[1]-=b[1];a[2]-=b[2];return a}c[0]=a[0]-b[0];c[1]=a[1]-b[1];c[2]=a[2]-b[2];return c};vec3.negate=function(a,b){b||(b=a);b[0]=-a[0];b[1]=-a[1];b[2]=-a[2];return b};vec3.scale=function(a,b,c){if(!c||a==c){a[0]*=b;a[1]*=b;a[2]*=b;return a}c[0]=a[0]*b;c[1]=a[1]*b;c[2]=a[2]*b;return c};
vec3.normalize=function(a,b){b||(b=a);var c=a[0],d=a[1],e=a[2],g=Math.sqrt(c*c+d*d+e*e);if(g){if(g==1){b[0]=c;b[1]=d;b[2]=e;return b}}else{b[0]=0;b[1]=0;b[2]=0;return b}g=1/g;b[0]=c*g;b[1]=d*g;b[2]=e*g;return b};vec3.cross=function(a,b,c){c||(c=a);var d=a[0],e=a[1];a=a[2];var g=b[0],f=b[1];b=b[2];c[0]=e*b-a*f;c[1]=a*g-d*b;c[2]=d*f-e*g;return c};vec3.length=function(a){var b=a[0],c=a[1];a=a[2];return Math.sqrt(b*b+c*c+a*a)};vec3.dot=function(a,b){return a[0]*b[0]+a[1]*b[1]+a[2]*b[2]};
vec3.direction=function(a,b,c){c||(c=a);var d=a[0]-b[0],e=a[1]-b[1];a=a[2]-b[2];b=Math.sqrt(d*d+e*e+a*a);if(!b){c[0]=0;c[1]=0;c[2]=0;return c}b=1/b;c[0]=d*b;c[1]=e*b;c[2]=a*b;return c};vec3.lerp=function(a,b,c,d){d||(d=a);d[0]=a[0]+c*(b[0]-a[0]);d[1]=a[1]+c*(b[1]-a[1]);d[2]=a[2]+c*(b[2]-a[2]);return d};vec3.str=function(a){return"["+a[0]+", "+a[1]+", "+a[2]+"]"};var mat3={};
mat3.create=function(a){var b=new glMatrixArrayType(9);if(a){b[0]=a[0];b[1]=a[1];b[2]=a[2];b[3]=a[3];b[4]=a[4];b[5]=a[5];b[6]=a[6];b[7]=a[7];b[8]=a[8];b[9]=a[9]}return b};mat3.set=function(a,b){b[0]=a[0];b[1]=a[1];b[2]=a[2];b[3]=a[3];b[4]=a[4];b[5]=a[5];b[6]=a[6];b[7]=a[7];b[8]=a[8];return b};mat3.identity=function(a){a[0]=1;a[1]=0;a[2]=0;a[3]=0;a[4]=1;a[5]=0;a[6]=0;a[7]=0;a[8]=1;return a};
mat3.transpose=function(a,b){if(!b||a==b){var c=a[1],d=a[2],e=a[5];a[1]=a[3];a[2]=a[6];a[3]=c;a[5]=a[7];a[6]=d;a[7]=e;return a}b[0]=a[0];b[1]=a[3];b[2]=a[6];b[3]=a[1];b[4]=a[4];b[5]=a[7];b[6]=a[2];b[7]=a[5];b[8]=a[8];return b};mat3.toMat4=function(a,b){b||(b=mat4.create());b[0]=a[0];b[1]=a[1];b[2]=a[2];b[3]=0;b[4]=a[3];b[5]=a[4];b[6]=a[5];b[7]=0;b[8]=a[6];b[9]=a[7];b[10]=a[8];b[11]=0;b[12]=0;b[13]=0;b[14]=0;b[15]=1;return b};
mat3.str=function(a){return"["+a[0]+", "+a[1]+", "+a[2]+", "+a[3]+", "+a[4]+", "+a[5]+", "+a[6]+", "+a[7]+", "+a[8]+"]"};var mat4={};mat4.create=function(a){var b=new glMatrixArrayType(16);if(a){b[0]=a[0];b[1]=a[1];b[2]=a[2];b[3]=a[3];b[4]=a[4];b[5]=a[5];b[6]=a[6];b[7]=a[7];b[8]=a[8];b[9]=a[9];b[10]=a[10];b[11]=a[11];b[12]=a[12];b[13]=a[13];b[14]=a[14];b[15]=a[15]}return b};
mat4.set=function(a,b){b[0]=a[0];b[1]=a[1];b[2]=a[2];b[3]=a[3];b[4]=a[4];b[5]=a[5];b[6]=a[6];b[7]=a[7];b[8]=a[8];b[9]=a[9];b[10]=a[10];b[11]=a[11];b[12]=a[12];b[13]=a[13];b[14]=a[14];b[15]=a[15];return b};mat4.identity=function(a){a[0]=1;a[1]=0;a[2]=0;a[3]=0;a[4]=0;a[5]=1;a[6]=0;a[7]=0;a[8]=0;a[9]=0;a[10]=1;a[11]=0;a[12]=0;a[13]=0;a[14]=0;a[15]=1;return a};
mat4.transpose=function(a,b){if(!b||a==b){var c=a[1],d=a[2],e=a[3],g=a[6],f=a[7],h=a[11];a[1]=a[4];a[2]=a[8];a[3]=a[12];a[4]=c;a[6]=a[9];a[7]=a[13];a[8]=d;a[9]=g;a[11]=a[14];a[12]=e;a[13]=f;a[14]=h;return a}b[0]=a[0];b[1]=a[4];b[2]=a[8];b[3]=a[12];b[4]=a[1];b[5]=a[5];b[6]=a[9];b[7]=a[13];b[8]=a[2];b[9]=a[6];b[10]=a[10];b[11]=a[14];b[12]=a[3];b[13]=a[7];b[14]=a[11];b[15]=a[15];return b};
mat4.determinant=function(a){var b=a[0],c=a[1],d=a[2],e=a[3],g=a[4],f=a[5],h=a[6],i=a[7],j=a[8],k=a[9],l=a[10],o=a[11],m=a[12],n=a[13],p=a[14];a=a[15];return m*k*h*e-j*n*h*e-m*f*l*e+g*n*l*e+j*f*p*e-g*k*p*e-m*k*d*i+j*n*d*i+m*c*l*i-b*n*l*i-j*c*p*i+b*k*p*i+m*f*d*o-g*n*d*o-m*c*h*o+b*n*h*o+g*c*p*o-b*f*p*o-j*f*d*a+g*k*d*a+j*c*h*a-b*k*h*a-g*c*l*a+b*f*l*a};
mat4.inverse=function(a,b){b||(b=a);var c=a[0],d=a[1],e=a[2],g=a[3],f=a[4],h=a[5],i=a[6],j=a[7],k=a[8],l=a[9],o=a[10],m=a[11],n=a[12],p=a[13],r=a[14],s=a[15],A=c*h-d*f,B=c*i-e*f,t=c*j-g*f,u=d*i-e*h,v=d*j-g*h,w=e*j-g*i,x=k*p-l*n,y=k*r-o*n,z=k*s-m*n,C=l*r-o*p,D=l*s-m*p,E=o*s-m*r,q=1/(A*E-B*D+t*C+u*z-v*y+w*x);b[0]=(h*E-i*D+j*C)*q;b[1]=(-d*E+e*D-g*C)*q;b[2]=(p*w-r*v+s*u)*q;b[3]=(-l*w+o*v-m*u)*q;b[4]=(-f*E+i*z-j*y)*q;b[5]=(c*E-e*z+g*y)*q;b[6]=(-n*w+r*t-s*B)*q;b[7]=(k*w-o*t+m*B)*q;b[8]=(f*D-h*z+j*x)*q;
b[9]=(-c*D+d*z-g*x)*q;b[10]=(n*v-p*t+s*A)*q;b[11]=(-k*v+l*t-m*A)*q;b[12]=(-f*C+h*y-i*x)*q;b[13]=(c*C-d*y+e*x)*q;b[14]=(-n*u+p*B-r*A)*q;b[15]=(k*u-l*B+o*A)*q;return b};mat4.toRotationMat=function(a,b){b||(b=mat4.create());b[0]=a[0];b[1]=a[1];b[2]=a[2];b[3]=a[3];b[4]=a[4];b[5]=a[5];b[6]=a[6];b[7]=a[7];b[8]=a[8];b[9]=a[9];b[10]=a[10];b[11]=a[11];b[12]=0;b[13]=0;b[14]=0;b[15]=1;return b};
mat4.toMat3=function(a,b){b||(b=mat3.create());b[0]=a[0];b[1]=a[1];b[2]=a[2];b[3]=a[4];b[4]=a[5];b[5]=a[6];b[6]=a[8];b[7]=a[9];b[8]=a[10];return b};mat4.toInverseMat3=function(a,b){var c=a[0],d=a[1],e=a[2],g=a[4],f=a[5],h=a[6],i=a[8],j=a[9],k=a[10],l=k*f-h*j,o=-k*g+h*i,m=j*g-f*i,n=c*l+d*o+e*m;if(!n)return null;n=1/n;b||(b=mat3.create());b[0]=l*n;b[1]=(-k*d+e*j)*n;b[2]=(h*d-e*f)*n;b[3]=o*n;b[4]=(k*c-e*i)*n;b[5]=(-h*c+e*g)*n;b[6]=m*n;b[7]=(-j*c+d*i)*n;b[8]=(f*c-d*g)*n;return b};
mat4.multiply=function(a,b,c){c||(c=a);var d=a[0],e=a[1],g=a[2],f=a[3],h=a[4],i=a[5],j=a[6],k=a[7],l=a[8],o=a[9],m=a[10],n=a[11],p=a[12],r=a[13],s=a[14];a=a[15];var A=b[0],B=b[1],t=b[2],u=b[3],v=b[4],w=b[5],x=b[6],y=b[7],z=b[8],C=b[9],D=b[10],E=b[11],q=b[12],F=b[13],G=b[14];b=b[15];c[0]=A*d+B*h+t*l+u*p;c[1]=A*e+B*i+t*o+u*r;c[2]=A*g+B*j+t*m+u*s;c[3]=A*f+B*k+t*n+u*a;c[4]=v*d+w*h+x*l+y*p;c[5]=v*e+w*i+x*o+y*r;c[6]=v*g+w*j+x*m+y*s;c[7]=v*f+w*k+x*n+y*a;c[8]=z*d+C*h+D*l+E*p;c[9]=z*e+C*i+D*o+E*r;c[10]=z*
g+C*j+D*m+E*s;c[11]=z*f+C*k+D*n+E*a;c[12]=q*d+F*h+G*l+b*p;c[13]=q*e+F*i+G*o+b*r;c[14]=q*g+F*j+G*m+b*s;c[15]=q*f+F*k+G*n+b*a;return c};mat4.multiplyVec3=function(a,b,c){c||(c=b);var d=b[0],e=b[1];b=b[2];c[0]=a[0]*d+a[4]*e+a[8]*b+a[12];c[1]=a[1]*d+a[5]*e+a[9]*b+a[13];c[2]=a[2]*d+a[6]*e+a[10]*b+a[14];return c};
mat4.multiplyVec4=function(a,b,c){c||(c=b);var d=b[0],e=b[1],g=b[2];b=b[3];c[0]=a[0]*d+a[4]*e+a[8]*g+a[12]*b;c[1]=a[1]*d+a[5]*e+a[9]*g+a[13]*b;c[2]=a[2]*d+a[6]*e+a[10]*g+a[14]*b;c[3]=a[3]*d+a[7]*e+a[11]*g+a[15]*b;return c};
mat4.translate=function(a,b,c){var d=b[0],e=b[1];b=b[2];if(!c||a==c){a[12]=a[0]*d+a[4]*e+a[8]*b+a[12];a[13]=a[1]*d+a[5]*e+a[9]*b+a[13];a[14]=a[2]*d+a[6]*e+a[10]*b+a[14];a[15]=a[3]*d+a[7]*e+a[11]*b+a[15];return a}var g=a[0],f=a[1],h=a[2],i=a[3],j=a[4],k=a[5],l=a[6],o=a[7],m=a[8],n=a[9],p=a[10],r=a[11];c[0]=g;c[1]=f;c[2]=h;c[3]=i;c[4]=j;c[5]=k;c[6]=l;c[7]=o;c[8]=m;c[9]=n;c[10]=p;c[11]=r;c[12]=g*d+j*e+m*b+a[12];c[13]=f*d+k*e+n*b+a[13];c[14]=h*d+l*e+p*b+a[14];c[15]=i*d+o*e+r*b+a[15];return c};
mat4.scale=function(a,b,c){var d=b[0],e=b[1];b=b[2];if(!c||a==c){a[0]*=d;a[1]*=d;a[2]*=d;a[3]*=d;a[4]*=e;a[5]*=e;a[6]*=e;a[7]*=e;a[8]*=b;a[9]*=b;a[10]*=b;a[11]*=b;return a}c[0]=a[0]*d;c[1]=a[1]*d;c[2]=a[2]*d;c[3]=a[3]*d;c[4]=a[4]*e;c[5]=a[5]*e;c[6]=a[6]*e;c[7]=a[7]*e;c[8]=a[8]*b;c[9]=a[9]*b;c[10]=a[10]*b;c[11]=a[11]*b;c[12]=a[12];c[13]=a[13];c[14]=a[14];c[15]=a[15];return c};
mat4.rotate=function(a,b,c,d){var e=c[0],g=c[1];c=c[2];var f=Math.sqrt(e*e+g*g+c*c);if(!f)return null;if(f!=1){f=1/f;e*=f;g*=f;c*=f}var h=Math.sin(b),i=Math.cos(b),j=1-i;b=a[0];f=a[1];var k=a[2],l=a[3],o=a[4],m=a[5],n=a[6],p=a[7],r=a[8],s=a[9],A=a[10],B=a[11],t=e*e*j+i,u=g*e*j+c*h,v=c*e*j-g*h,w=e*g*j-c*h,x=g*g*j+i,y=c*g*j+e*h,z=e*c*j+g*h;e=g*c*j-e*h;g=c*c*j+i;if(d){if(a!=d){d[12]=a[12];d[13]=a[13];d[14]=a[14];d[15]=a[15]}}else d=a;d[0]=b*t+o*u+r*v;d[1]=f*t+m*u+s*v;d[2]=k*t+n*u+A*v;d[3]=l*t+p*u+B*
v;d[4]=b*w+o*x+r*y;d[5]=f*w+m*x+s*y;d[6]=k*w+n*x+A*y;d[7]=l*w+p*x+B*y;d[8]=b*z+o*e+r*g;d[9]=f*z+m*e+s*g;d[10]=k*z+n*e+A*g;d[11]=l*z+p*e+B*g;return d};mat4.rotateX=function(a,b,c){var d=Math.sin(b);b=Math.cos(b);var e=a[4],g=a[5],f=a[6],h=a[7],i=a[8],j=a[9],k=a[10],l=a[11];if(c){if(a!=c){c[0]=a[0];c[1]=a[1];c[2]=a[2];c[3]=a[3];c[12]=a[12];c[13]=a[13];c[14]=a[14];c[15]=a[15]}}else c=a;c[4]=e*b+i*d;c[5]=g*b+j*d;c[6]=f*b+k*d;c[7]=h*b+l*d;c[8]=e*-d+i*b;c[9]=g*-d+j*b;c[10]=f*-d+k*b;c[11]=h*-d+l*b;return c};
mat4.rotateY=function(a,b,c){var d=Math.sin(b);b=Math.cos(b);var e=a[0],g=a[1],f=a[2],h=a[3],i=a[8],j=a[9],k=a[10],l=a[11];if(c){if(a!=c){c[4]=a[4];c[5]=a[5];c[6]=a[6];c[7]=a[7];c[12]=a[12];c[13]=a[13];c[14]=a[14];c[15]=a[15]}}else c=a;c[0]=e*b+i*-d;c[1]=g*b+j*-d;c[2]=f*b+k*-d;c[3]=h*b+l*-d;c[8]=e*d+i*b;c[9]=g*d+j*b;c[10]=f*d+k*b;c[11]=h*d+l*b;return c};
mat4.rotateZ=function(a,b,c){var d=Math.sin(b);b=Math.cos(b);var e=a[0],g=a[1],f=a[2],h=a[3],i=a[4],j=a[5],k=a[6],l=a[7];if(c){if(a!=c){c[8]=a[8];c[9]=a[9];c[10]=a[10];c[11]=a[11];c[12]=a[12];c[13]=a[13];c[14]=a[14];c[15]=a[15]}}else c=a;c[0]=e*b+i*d;c[1]=g*b+j*d;c[2]=f*b+k*d;c[3]=h*b+l*d;c[4]=e*-d+i*b;c[5]=g*-d+j*b;c[6]=f*-d+k*b;c[7]=h*-d+l*b;return c};
mat4.frustum=function(a,b,c,d,e,g,f){f||(f=mat4.create());var h=b-a,i=d-c,j=g-e;f[0]=e*2/h;f[1]=0;f[2]=0;f[3]=0;f[4]=0;f[5]=e*2/i;f[6]=0;f[7]=0;f[8]=(b+a)/h;f[9]=(d+c)/i;f[10]=-(g+e)/j;f[11]=-1;f[12]=0;f[13]=0;f[14]=-(g*e*2)/j;f[15]=0;return f};mat4.perspective=function(a,b,c,d,e){a=c*Math.tan(a*Math.PI/360);b=a*b;return mat4.frustum(-b,b,-a,a,c,d,e)};
mat4.ortho=function(a,b,c,d,e,g,f){f||(f=mat4.create());var h=b-a,i=d-c,j=g-e;f[0]=2/h;f[1]=0;f[2]=0;f[3]=0;f[4]=0;f[5]=2/i;f[6]=0;f[7]=0;f[8]=0;f[9]=0;f[10]=-2/j;f[11]=0;f[12]=-(a+b)/h;f[13]=-(d+c)/i;f[14]=-(g+e)/j;f[15]=1;return f};
mat4.lookAt=function(a,b,c,d){d||(d=mat4.create());var e=a[0],g=a[1];a=a[2];var f=c[0],h=c[1],i=c[2];c=b[1];var j=b[2];if(e==b[0]&&g==c&&a==j)return mat4.identity(d);var k,l,o,m;c=e-b[0];j=g-b[1];b=a-b[2];m=1/Math.sqrt(c*c+j*j+b*b);c*=m;j*=m;b*=m;k=h*b-i*j;i=i*c-f*b;f=f*j-h*c;if(m=Math.sqrt(k*k+i*i+f*f)){m=1/m;k*=m;i*=m;f*=m}else f=i=k=0;h=j*f-b*i;l=b*k-c*f;o=c*i-j*k;if(m=Math.sqrt(h*h+l*l+o*o)){m=1/m;h*=m;l*=m;o*=m}else o=l=h=0;d[0]=k;d[1]=h;d[2]=c;d[3]=0;d[4]=i;d[5]=l;d[6]=j;d[7]=0;d[8]=f;d[9]=
o;d[10]=b;d[11]=0;d[12]=-(k*e+i*g+f*a);d[13]=-(h*e+l*g+o*a);d[14]=-(c*e+j*g+b*a);d[15]=1;return d};mat4.str=function(a){return"["+a[0]+", "+a[1]+", "+a[2]+", "+a[3]+", "+a[4]+", "+a[5]+", "+a[6]+", "+a[7]+", "+a[8]+", "+a[9]+", "+a[10]+", "+a[11]+", "+a[12]+", "+a[13]+", "+a[14]+", "+a[15]+"]"};let quat4={};quat4.create=function(a){var b=new glMatrixArrayType(4);if(a){b[0]=a[0];b[1]=a[1];b[2]=a[2];b[3]=a[3]}return b};quat4.set=function(a,b){b[0]=a[0];b[1]=a[1];b[2]=a[2];b[3]=a[3];return b};
quat4.calculateW=function(a,b){var c=a[0],d=a[1],e=a[2];if(!b||a==b){a[3]=-Math.sqrt(Math.abs(1-c*c-d*d-e*e));return a}b[0]=c;b[1]=d;b[2]=e;b[3]=-Math.sqrt(Math.abs(1-c*c-d*d-e*e));return b};quat4.inverse=function(a,b){if(!b||a==b){a[0]*=1;a[1]*=1;a[2]*=1;return a}b[0]=-a[0];b[1]=-a[1];b[2]=-a[2];b[3]=a[3];return b};quat4.length=function(a){var b=a[0],c=a[1],d=a[2];a=a[3];return Math.sqrt(b*b+c*c+d*d+a*a)};
quat4.normalize=function(a,b){b||(b=a);var c=a[0],d=a[1],e=a[2],g=a[3],f=Math.sqrt(c*c+d*d+e*e+g*g);if(f==0){b[0]=0;b[1]=0;b[2]=0;b[3]=0;return b}f=1/f;b[0]=c*f;b[1]=d*f;b[2]=e*f;b[3]=g*f;return b};quat4.multiply=function(a,b,c){c||(c=a);var d=a[0],e=a[1],g=a[2];a=a[3];var f=b[0],h=b[1],i=b[2];b=b[3];c[0]=d*b+a*f+e*i-g*h;c[1]=e*b+a*h+g*f-d*i;c[2]=g*b+a*i+d*h-e*f;c[3]=a*b-d*f-e*h-g*i;return c};
quat4.multiplyVec3=function(a,b,c){c||(c=b);var d=b[0],e=b[1],g=b[2];b=a[0];var f=a[1],h=a[2];a=a[3];var i=a*d+f*g-h*e,j=a*e+h*d-b*g,k=a*g+b*e-f*d;d=-b*d-f*e-h*g;c[0]=i*a+d*-b+j*-h-k*-f;c[1]=j*a+d*-f+k*-b-i*-h;c[2]=k*a+d*-h+i*-f-j*-b;return c};quat4.toMat3=function(a,b){b||(b=mat3.create());var c=a[0],d=a[1],e=a[2],g=a[3],f=c+c,h=d+d,i=e+e,j=c*f,k=c*h;c=c*i;var l=d*h;d=d*i;e=e*i;f=g*f;h=g*h;g=g*i;b[0]=1-(l+e);b[1]=k-g;b[2]=c+h;b[3]=k+g;b[4]=1-(j+e);b[5]=d-f;b[6]=c-h;b[7]=d+f;b[8]=1-(j+l);return b};
quat4.toMat4=function(a,b){b||(b=mat4.create());var c=a[0],d=a[1],e=a[2],g=a[3],f=c+c,h=d+d,i=e+e,j=c*f,k=c*h;c=c*i;var l=d*h;d=d*i;e=e*i;f=g*f;h=g*h;g=g*i;b[0]=1-(l+e);b[1]=k-g;b[2]=c+h;b[3]=0;b[4]=k+g;b[5]=1-(j+e);b[6]=d-f;b[7]=0;b[8]=c-h;b[9]=d+f;b[10]=1-(j+l);b[11]=0;b[12]=0;b[13]=0;b[14]=0;b[15]=1;return b};quat4.slerp=function(a,b,c,d){d||(d=a);var e=c;if(a[0]*b[0]+a[1]*b[1]+a[2]*b[2]+a[3]*b[3]<0)e=-1*c;d[0]=1-c*a[0]+e*b[0];d[1]=1-c*a[1]+e*b[1];d[2]=1-c*a[2]+e*b[2];d[3]=1-c*a[3]+e*b[3];return d};
quat4.str=function(a){return"["+a[0]+", "+a[1]+", "+a[2]+", "+a[3]+"]"};
export let J3D = {};

J3D.debug = true;

J3D.LightmapAtlas = [];

// Read only global settings
J3D.SHADER_MAX_LIGHTS = 4;
J3D.RENDER_AS_OPAQUE = 0;
J3D.RENDER_AS_TRANSPARENT = 1;
J3D.Color = function(r, g, b, a){
	var that = this;
	this.r = r || 0;
	this.g = g || 0;
	this.b = b || 0;
	this.a = a || 0;
	
	this.rgba = function() {
		return [that.r, that.g, that.b, that.a];
	}
	
	this.rgb = function() {
		return [that.r, that.g, that.b];
	}
	
	this.toUniform = function(type){
		if(type == gl.FLOAT_VEC3) return this.rgb();
		else return this.rgba();
	}
}

J3D.Color.white = new J3D.Color(1,1,1,1);
J3D.Color.black = new J3D.Color(0,0,0,1);

J3D.Color.red =   new J3D.Color(1,0,0,1);
J3D.Color.green = new J3D.Color(0,1,0,1);
J3D.Color.blue =  new J3D.Color(0,0,1,1);
var v2 = function(x, y){
	this.x = x || 0;
	this.y = y || 0;
}

v2.prototype.set = function(x, y){
	this.x = x || 0;
	this.y = y || 0;
};

v2.prototype.xy = function() {
	return [this.x, this.y];
}

v2.prototype.isOne = function() {
	return this.x == 1 && this.y == 1;
}

v2.prototype.isZero = function() {
	return this.x == 0 && this.y == 0;
}


v2.ZERO = function() { return new v2(0, 0); }
v2.ONE = function() { return new v2(1, 1); }
v2.random = function() { return new v2(Math.random() * 2 - 1, Math.random() * 2 - 1); }
export function v3(x, y, z){
	this.x = x || 0;
	this.y = y || 0;
	this.z = z || 0;
}

v3.prototype.set = function(x, y, z){
	this.x = x || 0;
	this.y = y || 0;
	this.z = z || 0;
};

v3.prototype.magSq = function() { return this.x * this.x + this.y * this.y + this.z * this.z; };

v3.prototype.mag = function() { return Math.sqrt( this.magSq() ); };

v3.prototype.mul = function(s) {
	return new v3(this.x * s, this.y * s, this.z * s);
}

v3.prototype.neg = function() {
	this.x = -this.x;
	this.y = -this.y;
	this.z = -this.z;
	return this;
}

v3.prototype.norm = function() {
	var m = 1 / this.mag();
	this.set(this.x * m, this.y * m, this.z * m);
	return this;
}

v3.prototype.cp = function() {
	return new v3(this.x, this.y, this.z);
}

v3.prototype.add = function(b) {
	return v3.add(this, b);
}

v3.prototype.xyz = function() {
	return [this.x, this.y, this.z];
}

v3.prototype.toUniform = function() {
	return this.xyz();
}

v3.add = function(a, b) {
	var c = new v3(a.x, a.y, a.z);
	c.x += b.x;
	c.y += b.y;
	c.z += b.z;

	return c;
}

v3.prototype.sub = function(b) {
	return v3.sub(this, b);
}

v3.sub = function(a, b) {
	var c = new v3(a.x, a.y, a.z);
	c.x -= b.x;
	c.y -= b.y;
	c.z -= b.z;

	return c;
}

v3.dot = function(a, b) {
	return a.x * b.x + a.y * b.y + a.z * b.z;
}

v3.cross = function(a, b) {
	return new v3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);
}

v3.ZERO = function() { return new v3(0, 0, 0); }
v3.ONE = function() { return new v3(1, 1, 1); }
v3.RIGHT = function() { return new v3(1, 0, 0); }
v3.UP = function() { return new v3(0, 1, 0); }
v3.FORWARD = function() { return new v3(0, 0, 1); }
v3.random = function() { return new v3(Math.random() * 2 - 1, Math.random() * 2 - 1, Math.random() * 2 - 1); }
var m44 = function(){
	this.array = [];//new Float32Array(16);
	this.identity();
}

m44.prototype.identity = function(){
    this.n11 = 1;
    this.n12 = 0;
    this.n13 = 0;
    this.n14 = 0;
	
    this.n21 = 0;
    this.n22 = 1;
    this.n23 = 0;
    this.n24 = 0;
	
    this.n31 = 0;
    this.n32 = 0;
    this.n33 = 1;
    this.n34 = 0;
	
    this.n41 = 0;
    this.n42 = 0;
    this.n43 = 0;
    this.n44 = 1;
}

// based on https://github.com/mrdoob/three.js/blob/master/src/core/Matrix4.js
m44.prototype.ortho = function(left, right, top, bottom, near, far) {

	var x, y, z, w, h, p;

	w = right - left;
	h = bottom - top;
	p = far - near;
	x = ( right + left ) / w;
	y = ( bottom + top ) / h;
	z = ( far + near ) / p;

	this.n11 = 2 / w;
	this.n14 = -x;
	this.n22 = -2 / h; 
	this.n24 = y;
	this.n33 = 2 / p; 
	this.n34 = -z;

	this.makeArray();
	
	//console.log(this.array.join(","));
}

m44.prototype.perspective = function(fov, aspect, near, far){
    var t = near * Math.tan(fov * Math.PI / 360);
	var n = far - near;

	this.n11 = near / (t * aspect);
	this.n22 = near / t;
	this.n33 = -(far + near) / n; 
	this.n34 = -(2 * far * near) / n;
	this.n43 = -1;
	this.n44 = 0;
	
	this.makeArray();
};

m44.prototype.makeArray = function(){
	this.array[0] = this.n11;
	this.array[1] = this.n21;
	this.array[2] = this.n31;
	this.array[3] = this.n41;
	
	this.array[4] = this.n12;
	this.array[5] = this.n22;
	this.array[6] = this.n32;
	this.array[7] = this.n42;
	
	this.array[8] = this.n13;
	this.array[9] = this.n23;
	this.array[10] = this.n33;
	this.array[11] = this.n43;
	
	this.array[12] = this.n14;
	this.array[13] = this.n24;
	this.array[14] = this.n34;
	this.array[15] = this.n44;
}

m44.prototype.toArray = function(){
	return this.array;
}
export let gl = null;

var i = 0;
J3D.Engine = function(canvas, j3dSettings, webglSettings) {	
console.log(i++);
	var cv = (canvas) ? canvas : document.createElement("canvas");
	
	if (!canvas) {
console.log(i++);
		var resolution = (j3dSettings && j3dSettings.resolution) ? j3dSettings.resolution : 1;
		cv.width = window.innerWidth / resolution;
		cv.height = window.innerHeight / resolution;
		cv.style.width = "100%";
		cv.style.height = "100%";
		document.body.appendChild(cv);
	}

	try {
console.log(i++);
		gl = cv.getContext("experimental-webgl", webglSettings);
		gl.viewportWidth = cv.width;
		gl.viewportHeight = cv.height;
	} 
	catch (e) {
console.log(i++);
		j3dlog("ERROR. Getting webgl context failed!");
		return;
	}
	
console.log(i++);
	this.setClearColor(J3D.Color.black);
	gl.viewport(0, 0, gl.viewportWidth, gl.viewportHeight);	
	gl.enable(gl.CULL_FACE);
	gl.frontFace(gl.CW);

console.log(i++);
	this.shaderAtlas = new J3D.ShaderAtlas();
	this.scene = new J3D.Scene();
	this.camera; // it is a J3D.Transform
	
console.log(i++);
	this.canvas = cv;
	
console.log(i++);
	this._opaqueMeshes = [];
	this._transparentMeshes = [];
	this._lights = [];
	
console.log(i++);
	this.gl = gl;
console.log(i++);
}

J3D.Engine.prototype.setClearColor = function(c) {
	gl.clearColor(c.r, c.g, c.b, c.a);
}

J3D.Engine.prototype.render = function(){
	J3D.Time.tick();
	
	gl.clear(gl.COLOR_BUFFER_BIT | gl.DEPTH_BUFFER_BIT);
	
	if(this.scene.numChildren > 0) this.renderScene();
}

J3D.Engine.prototype.renderScene = function(){

	

	// 3. Clear collecions
	this._opaqueMeshes.length = 0;
	this._transparentMeshes.length = 0;
	this._lights.length = 0;

	// 4. Update all transforms recursively
	for(var i = 0; i < this.scene.numChildren; i++) {
		this.updateTransform(this.scene.childAt(i), null);
	}
	
	// 5. Calculate camera inverse matrix and it's world position
	this.camera.updateInverseMat();
	
	// 6. Render sky box (if any)
	if(this.scene.skybox) {
		gl.depthMask(false);
		this.scene.skybox.renderer.mid = this.camera.camera.near + (this.camera.camera.far-this.camera.camera.near)/2;	
		this.renderObject(this.scene.skybox);
		gl.depthMask(true);	
	}
	
	// 7. Calculate global positions for all lights
	for (var i = 0; i < this._lights.length; i++) {
		var t = this._lights[i];
		t.updateWorldPosition();
	}

	// 8. Render opaque meshes
	gl.disable(gl.BLEND);
	gl.enable(gl.DEPTH_TEST);
	for(var i = 0; i < this._opaqueMeshes.length; i++){
		this.renderObject(this._opaqueMeshes[i]);
	}

	// 9. Render transparent meshes	(TODO: sort before rendering)
	gl.disable(gl.DEPTH_TEST);
	gl.enable(gl.BLEND);
	for(var i = 0; i < this._transparentMeshes.length; i++) {
		var t = this._transparentMeshes[i];
		var srcFactor = (t.geometry.srcFactor != null) ? t.geometry.srcFactor : gl.SRC_ALPHA;
		var dstFactor = (t.geometry.dstFactor != null) ? t.geometry.dstFactor : gl.ONE;
		gl.blendFunc(srcFactor, dstFactor);
		this.renderObject(t);
	}

	// #DEBUG Monitor the amount of shaders created
	// console.log( this.shaderAtlas.shaderCount );

	// gl.flush();
}

J3D.Engine.prototype.renderObject = function(t) {
	var s = this.shaderAtlas.getShader(t.renderer);

	gl.useProgram(s);
	
	// Setup standard uniforms and attributes
	if(s.uniforms.uTime) 
		gl.uniform1f(s.uniforms.uTime.location, J3D.Time.time);	
		
	if(s.uniforms.pMatrix)
		gl.uniformMatrix4fv(s.uniforms.pMatrix.location, false, this.camera.camera.projectionMat.toArray() );
		
	if(s.uniforms.vMatrix) 	
		gl.uniformMatrix4fv(s.uniforms.vMatrix.location, false, this.camera.inverseMat);
		
	if(s.uniforms.mMatrix) 
		gl.uniformMatrix4fv(s.uniforms.mMatrix.location, false, t.globalMatrix);
		
	if(s.uniforms.nMatrix) 
		gl.uniformMatrix3fv(s.uniforms.nMatrix.location, false, t.normalMatrix);
			
	if(s.uniforms.uAmbientColor) 
		gl.uniform3fv(s.uniforms.uAmbientColor.location, this.scene.ambient.rgb());
		
	if(s.uniforms.uEyePosition) 
		gl.uniform3fv(s.uniforms.uEyePosition.location, this.camera.worldPosition.xyz());
			
	if(s.uniforms.uTileOffset) 
		gl.uniform4fv(s.uniforms.uTileOffset.location, t.getTileOffset());
	
	J3D.ShaderUtil.setLights(s, this._lights);

	for(var i = 0; i < t.geometry.arrays.length; i++) {
		var vbo = t.geometry.arrays[i];	
		if(s.attributes[vbo.name] != null) {
			gl.bindBuffer(gl.ARRAY_BUFFER, vbo.buffer);
			gl.vertexAttribPointer(s.attributes[vbo.name], vbo.itemSize, gl.FLOAT, false, 0, 0);
		}
	}
		
	// Setup renderers custom uniforms and attributes
	t.renderer.setup(s, t);

	var cull = t.renderer.cullFace || gl.BACK;			
	gl.cullFace(cull);
	
	var mode = (t.renderer.drawMode != null) ? t.renderer.drawMode : gl.TRIANGLES;
	
	if (t.geometry.hasElements) {
		gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER, t.geometry.elements.buffer);
		gl.drawElements(mode, t.geometry.elements.size, gl.UNSIGNED_SHORT, 0);
	} else {
		gl.drawArrays(mode, 0, t.geometry.size);
	}
}

J3D.Engine.prototype.updateTransform = function(t, p){
	t.updateWorld(p);
	
	for (var j = 0; j < t.numChildren; j++) {
		this.updateTransform(t.childAt(j), t);
	}
	
	if(!t.enabled) return;
	
	if (t.renderer && t.geometry) {	
		if(t.geometry.renderMode == J3D.RENDER_AS_TRANSPARENT) 
			this._transparentMeshes.push(t);	
		else 
			this._opaqueMeshes.push(t);
	}
	
	if (t.light) {
		this._lights.push(t);
	}
}










// Constructor parameters for perspective: { type, fov, near, far, aspect }
// Constructor parameters for ortho: { type, left, right, top, bottom, near, far }


J3D.PERSPECTIVE = 0; // <- if not provided, this one is default
J3D.ORTHO = 1;


J3D.Camera = function(params){
	if(!params) params = {};
	
	if(!params.type) params.type = J3D.PERSPECTIVE;
	
	if(!params.near) params.near = 1;
	if(!params.far) params.far = 1000;
	
	if(params.type == J3D.PERSPECTIVE) {
		if(!params.fov) params.fov = 45;
		if(!params.aspect) params.aspect = gl.viewportWidth / gl.viewportHeight;
	} else {
		if(params.left == null) params.left = 0;
		if(params.right == null) params.right = 1;
		if(params.top == null) params.top = 0;
		if(params.bottom == null) params.bottom = 1;
	}
	
	this.near = params.near;
	this.far = params.far;

	this.projectionMat = new m44();
	
	if(params.type == J3D.PERSPECTIVE) 
		this.projectionMat.perspective(params.fov, params.aspect, params.near, params.far);
	else 
		this.projectionMat.ortho(params.left, params.right, params.top, params.bottom, params.near, params.far);	
}


J3D.Light = function(t){
	this.type = t || J3D.NONE;
	this.direction = v3.ZERO();
	this.color = J3D.Color.white;
}

J3D.NONE = parseInt(0); // For shader internal use
J3D.DIRECT = parseInt(1);
J3D.POINT = parseInt(2);
// J3D.SPOT = 3;
J3D.Geometry = function(){
	this.renderMode = J3D.RENDER_AS_OPAQUE;
	this.arrays = [];
	this.elements;
	this.hasElements = false;
	this.size;
}

J3D.Geometry.prototype.setTransparency = function(transparency, srcFactor, dstFactor) {
	if(!transparency) {
		this.renderMode = J3D.RENDER_AS_OPAQUE;
	} else {
		this.renderMode = J3D.RENDER_AS_TRANSPARENT;
		this.srcFactor = srcFactor;
		this.dstFactor = dstFactor;
	}
}

J3D.Geometry.prototype.addArray = function(name, data, itemSize, type, usage) {
	if(!type) type = gl.FLOAT;
	if(!usage) usage = gl.STATIC_DRAW;
	var vbo = new J3D.Geometry.Attribute(name, data, itemSize, type, usage, gl.ARRAY_BUFFER);
	this.arrays.push(vbo);
	this.size = vbo.size;
	return vbo;
}

J3D.Geometry.prototype.replaceArray = function(vbo, data, usage) {
	if(!usage) usage = gl.STATIC_DRAW;
	vbo.data = data;
	gl.bindBuffer(gl.ARRAY_BUFFER, vbo.buffer);
    gl.bufferData(gl.ARRAY_BUFFER, data, usage);
}

J3D.Geometry.prototype.addElement = function(data, type, usage) {
	if(!type) type = gl.UNSIGNED_SHORT;
	if(!usage) usage = gl.STATIC_DRAW;
	this.elements = new J3D.Geometry.Attribute("", data, 0, type, usage, gl.ELEMENT_ARRAY_BUFFER);
	this.hasElements = true;
}

J3D.Geometry.Attribute = function(name, data, itemSize, type, usage, target) {
	this.name = name;
	this.data = data;
	
	this.buffer = gl.createBuffer();
	gl.bindBuffer(target, this.buffer);
	gl.bufferData(target, data, usage);
	
	this.size = (itemSize > 0) ? data.length / itemSize : data.length;
	this.itemSize = itemSize;
	this.type = type;
}
/*
 *  A Mesh is a structured geometry coming from and external source (either a JSON file or generated with code)
 *  
 *  Typically a Mesh is designed to hold data about 3D objects. It has a primary set of attributes that will be interpreted by name:
 *  vertices (3 x float), colors (4 x float), normals (3 x float), uv1 (2 x float), uv2 (2 x float) - none is mandatory.
 * 
 *  If an attribute named "tris" is present it will be interpreted as the elements array.
 *  
 *  WARNING: Other attributes in the "source" will be ignored.
 *  Mesh extends Geometry, so more attributes can be added manually if necessary.
 */
J3D.Mesh = function(source){
	J3D.Geometry.call( this );
	
	this.hasUV1 = false;
	
	for(var attr in source) {
		switch (attr) {
			case "vertices":
				this.vertexPositionBuffer = this.addArray("aVertexPosition", new Float32Array(source[attr]), 3);
			break;
			case "colors":
				if(source[attr].length > 0) this.addArray("aVertexColor", new Float32Array(source[attr]), 4);
			break;
			case "normals":
				if(source[attr].length > 0) 
					this.vertexNormalBuffer = this.addArray("aVertexNormal", new Float32Array(source[attr]), 3);
				else 
					this.vertexNormalBuffer = this.addArray("aVertexNormal", new Float32Array(this.size * 3), 3);
			break;
			case "uv1":
				if(source[attr].length > 0) this.addArray("aTextureCoord", new Float32Array(source[attr]), 2);
				else this.addArray("aTextureCoord", new Float32Array(this.size * 2), 2);
				this.hasUV1 = true;
			break;
			case "uv2":
				if(source[attr].length > 0) this.addArray("aTextureCoord2", new Float32Array(source[attr]), 2);
			break;
			case "tris":
				if(source[attr].length > 0) this.addElement(new Uint16Array(source[attr]));
			break;
			default:
				console.log("WARNING! Unknown attribute: " + attr);
			break;
		}
	}

	this.flip = function(){
		var tv = [];
		var vertices = this.vertexPositionBuffer.data;
		for (var i = 0; i < vertices.length; i += 3) {
			tv.push(vertices[i], vertices[i + 2], vertices[i + 1]);
		}
		vertices = new Float32Array(tv);
		
		var tn = [];
		var normals = this.vertexNormalBuffer.data;
		for (var i = 0; i < normals.length; i += 3) {
			var v = new v3(normals[i], normals[i + 1], normals[i + 2])
			v.neg();
			tn = tn.concat(v.xyz());
		}
		normals = new Float32Array(tn);
		
		this.replaceArray(this.vertexPositionBuffer, vertices);
		this.replaceArray(this.vertexNormalBuffer, normals);
		
		return this;
	}
}

J3D.Mesh.prototype = new J3D.Geometry();
J3D.Mesh.prototype.constructor = J3D.Mesh;
J3D.Mesh.prototype.supr = J3D.Geometry.prototype;

J3D.Scene = function() {
	var that = this;
	var children = [];
	
	this.ambient = J3D.Color.black;
	this.numChildren;
	this.skybox;
	
	this.add = function() {
		var fa;
		for (var i = 0; i < arguments.length; i++) {
			var t = arguments[i];
			if(!fa) fa = t;
			children.push(t);
			t.parent = null;
			that.numChildren = children.length;
		}
		return fa;
	}
	
	this.childAt = function(i){
		return children[i];
	}

	this.addSkybox = function(cubemap) {
		this.skybox = new J3D.Transform();
		this.skybox.renderer = new J3D.BuiltinShaders.fetch("Skybox");
		this.skybox.renderer.uCubemap = cubemap;
		this.skybox.geometry = J3D.Primitive.Cube(1, 1, 1).flip();
	}
}

J3D.Scene.prototype.find = function(path) {
	var p = path.split("/");
	
	for(var i = 0; i < this.numChildren; i++) {
		if(this.childAt(i).name == p[0]) {
			if(p.length == 1) return this.childAt(i);
			else return this.childAt(i).find(p.slice(1));
		}
	}
	
	return null;
}
J3D.Loader = {};

J3D.Loader.loadJSON = function(path, onLoadedFunc){
/*
	var request = new XMLHttpRequest();
	request.open("GET", path);

	request.onreadystatechange = function(){
		if (request.readyState == 4) {
			onLoadedFunc.call(null, JSON.parse(request.responseText));
		}
	};

	request.send();
*/
	onLoadedFunc.call (null, JSON.parse (ejsfs.readFileSync (path, 'utf-8')));
}

J3D.Loader.parseJSONScene = function(jscene, jmeshes, engine) {

	engine.scene.ambient = J3D.Loader.fromObject(J3D.Color, jscene.ambient);
	engine.setClearColor( J3D.Loader.fromObject(J3D.Color, jscene.background) );

	for(var txs in jscene.textures) {
		var tx = new J3D.Texture( jscene.path + jscene.textures[txs].file );
		jscene.textures[txs] = tx;
	}

	for(var ms in jscene.materials) {
		var m = jscene.materials[ms];
		m = J3D.Loader.fetchShader(m.type, m);
		m.color = J3D.Loader.fromObject(J3D.Color, m.color);
		if(m.textureTile) m.textureTile = J3D.Loader.v2FromArray(m.textureTile);
		if(m.textureOffset) m.textureOffset = J3D.Loader.v2FromArray(m.textureOffset);

		if (m.colorTexture) {
			m.colorTexture = jscene.textures[m.colorTexture];
			m.hasColorTexture = true;
		}

		jscene.materials[ms] = m;
	}

	for(var lgs in jscene.lights) {
		var lg = jscene.lights[lgs];
		lg = J3D.Loader.fromObject(J3D.Light, lg);
		lg.color = J3D.Loader.fromObject(J3D.Color, lg.color);
		if(lg.direction) lg.direction = J3D.Loader.v3FromArray(lg.direction);
		jscene.lights[lgs] = lg;
	}

	for(var cms in jscene.cameras) {
		var cm = jscene.cameras[cms];
		cm = new J3D.Camera({fov:cm.fov, near:cm.near, far:cm.far});
		jscene.cameras[cms] = cm;
	}

	for(var i = 0; i < jscene.transforms.length; i++) {
		var t = jscene.transforms[i];
		t = J3D.Loader.fromObject(J3D.Transform, t);
		t.position = J3D.Loader.v3FromArray(t.position);
		t.rotation = J3D.Loader.v3FromArray(t.rotation);

		if(t.renderer) t.renderer = jscene.materials[t.renderer];
		if(t.mesh) t.geometry = new J3D.Mesh(jmeshes[t.mesh]);
		if(t.light) t.light = jscene.lights[t.light];

		if (t.camera) {
			//jscene.cameras[t.camera].transform = t;
			t.camera = jscene.cameras[t.camera];
			engine.camera = t;
		}

		jscene.transforms[i] = t;
	}

	var findByUID = function(n) {
		for (var i = 0; i < jscene.transforms.length; i++) {
			if(jscene.transforms[i].uid == n) return jscene.transforms[i];
		}
	}

	for(var i = 0; i < jscene.transforms.length; i++) {
		var t = jscene.transforms[i];
		if (t.parent != null) {
			t.parent = findByUID(t.parent);
			t.parent.add(t);
		} else {
			engine.scene.add(t);
		}
	}
}

J3D.Loader.fetchShader = function(type, obj){
	var t = J3D.BuiltinShaders.fetch(type);
	for(var p in obj) t[p] = obj[p];
	return t;
}

J3D.Loader.fromObject = function(type, obj){
	var t = new type();
	for(var p in obj) t[p] = obj[p];
	return t;
}

J3D.Loader.v2FromArray = function(arr){
	return new v2(arr[0], arr[1]);
}

J3D.Loader.v3FromArray = function(arr){
	return new v3(arr[0], arr[1], arr[2]);
}

J3D.Loader.loadGLSL = function(path, onLoadedFunc){
  /*
	var request = new XMLHttpRequest();
	request.open("GET", path);

	request.onreadystatechange = function(){
		if (request.readyState == 4) {
			onLoadedFunc.call(null, J3D.ShaderUtil.parseGLSL(request.responseText));
		}
	}

	request.send();
*/
	onLoadedFunc.call(null, J3D.ShaderUtil.parseGLSL(ejsfs.readFileSync (path, 'utf-8')));
}
J3D.ShaderAtlas = function(){
	this.shaders = {};
	this.programs = {};
	this.shaderCount = 0;
}

J3D.ShaderAtlas.prototype.compileShaderSource = function(name, src, type, meta){
	var isrc;
	
	var ci = meta.common || "";
	if(meta.includes && meta.includes.length > 0) {
		for(var i = 0; i < meta.includes.length; i++) {
//			j3dlog("Adding common include: " + meta.includes[i]);
			ci += J3D.ShaderSource[meta.includes[i]];
		}
	}
	
	if (type == gl.VERTEX_SHADER) {
		var vi = "";
		if(meta.vertexIncludes && meta.vertexIncludes.length > 0) {
			for(var i = 0; i < meta.vertexIncludes.length; i++) {
//				j3dlog("Adding vert include: " + meta.vertexIncludes[i])
				vi += J3D.ShaderSource[meta.vertexIncludes[i]];
			}
		}
		isrc = ci + vi + src;
	} else {
		var fi = "";
		if(meta.fragmentIncludes && meta.fragmentIncludes.length > 0) {
			for(var i = 0; i < meta.fragmentIncludes.length; i++) {
//				j3dlog("Adding frag include: " + meta.fragmentIncludes[i]);
				fi += J3D.ShaderSource[meta.fragmentIncludes[i]];
			}
		}
		isrc = ci + fi + src;
	}	
	
	var shader = gl.createShader(type);
	gl.shaderSource(shader, isrc);
    gl.compileShader(shader);
 
    if (!gl.getShaderParameter(shader, gl.COMPILE_STATUS)) {
		j3dlog("ERROR. Shader compile error: " + gl.getShaderInfoLog(shader));
    }
	
	this.programs[name] = shader;
}

J3D.ShaderAtlas.prototype.linkShader = function(renderer){
	var name = renderer.name;
	
	var vertName = name + "Vert";
	var fragName = name + "Frag";
	
	var vertexShader = this.programs[vertName];
	var fragmentShader = this.programs[fragName];
	
	var program = gl.createProgram();
	gl.attachShader(program, vertexShader);
	gl.attachShader(program, fragmentShader);
	gl.linkProgram(program);
 
	if (!gl.getProgramParameter(program, gl.LINK_STATUS)) {
		console.log("Error linking program " + name);
	}
	
	gl.useProgram(program);
	
	var tid = 0;
	program.uniforms = {};
	var numUni = gl.getProgramParameter(program, gl.ACTIVE_UNIFORMS);
	for(var i = 0; i < numUni; i++) {
		var acUni = gl.getActiveUniform(program, i);
		program.uniforms[acUni.name] = acUni;
		program.uniforms[acUni.name].location = gl.getUniformLocation(program, acUni.name);
		if (J3D.ShaderUtil.isTexture(acUni.type)) {
			program.uniforms[acUni.name].texid = tid;
			tid++;
		}
	}
	
	program.attributes = {};
	var numAttr = gl.getProgramParameter(program, gl.ACTIVE_ATTRIBUTES);
	for(var i = 0; i < numAttr; i++) {
		var acAttr = gl.getActiveAttrib(program, i);
		program.attributes[acAttr.name] = gl.getAttribLocation(program, acAttr.name);
		gl.enableVertexAttribArray(program.attributes[acAttr.name]);
	}

	this.shaderCount++;
	this.shaders[name] = program;
}

J3D.ShaderAtlas.prototype.getShader = function (r) {
	if(!this.shaders[r.name]) {
		this.compileShaderSource(r.name + "Vert", r.vertSource(), gl.VERTEX_SHADER, r.metaData);
		this.compileShaderSource(r.name + "Frag", r.fragSource(), gl.FRAGMENT_SHADER, r.metaData);
		this.linkShader(r);
	}
	
	return this.shaders[r.name];
}











J3D.Texture = function(source, params){ // <- use this to pass parameters of the texture
	var that = this;
	this.tex = gl.createTexture();

	if(!params) params = {};
	this.loaded = false;
	this.isVideo = false;

	this.onLoad = params.onLoad;
	this.mipmap = (params.mipmap != null) ? params.mipmap : true;
	this.flip = (params.flip != null) ? params.flip : true;
	this.wrapMode = params.wrapMode || gl.REPEAT;
	this.magFilter = params.magFilter || gl.LINEAR;
	this.minFilter = params.minFilter || gl.LINEAR_MIPMAP_NEAREST;


	var isPOT = function(x, y){
	    return x > 0 && y > 0 && (x & (x - 1)) == 0 && (y & (y - 1)) == 0;
	}

	var setupTexture = function(){
		//j3dlog(that.src.width + " x " + that.src.height + " isPOT: " + isPOT(that.src.width, that.src.height));

		var p = that.src && isPOT(that.src.width, that.src.height);

		gl.bindTexture(gl.TEXTURE_2D, that.tex);
		gl.pixelStorei(gl.UNPACK_FLIP_Y_WEBGL, that.flip);
		gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, gl.RGBA, gl.UNSIGNED_BYTE, that.src);
		gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, that.magFilter);

		if(p) gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, that.minFilter);
		else gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR);

		if (p) {
			gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, that.wrapMode);
			gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, that.wrapMode);
		} else {
			if(that.wrapMode != gl.CLAMP_TO_EDGE) j3dlog("WARNING! Texture: " + source + " : only CLAMP_TO_EDGE wrapMode is supported for non-power-of-2 textures.");
			gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
			gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
		}

		if(that.mipmap && p) gl.generateMipmap(gl.TEXTURE_2D);
		gl.bindTexture(gl.TEXTURE_2D, null);

		if(that.onLoad) that.onLoad.call();

		that.loaded = true;
	}

	var loadImage = function(src){
/*
		that.src = new Image();
    	that.src.onload = function() {
			setupTexture();
    	}
		that.src.src = src;
*/
	  console.log ("loading image from " + ejspath.resolve (process.cwd(), src));
	  that.src = uikit.UIImage.imageWithContentsOfFile (ejspath.resolve (process.cwd(), src));
	  console.log ("that.src = " + (that.src || "<null>"));
	  setupTexture();
	}

	var loadVideo = function(src){
		that.isVideo = true;
		that.src = document.createElement('video');
	    that.src.src = src;
		that.src.preload = 'auto';
		that.src.addEventListener( "canplaythrough", function() {
			that.src.play();
			//j3dlog("Video " + src + " can play through");
			//document.body.appendChild(that.src);
			setupTexture();

		});

		that.src.load();
	}

	if (typeof(source) == "string") {
		var ext = source.substring(source.lastIndexOf(".") + 1).toLowerCase();

		//j3dlog("Extension: " + ext);

		switch(ext) {
			case "jpg":
			case "png":
			case "gif":
				loadImage(source);
				break;
			case "mp4":
			case "webm":
			case "ogv":
				loadVideo(source);
				break;
		}

	} else if(!!source.getContext) {
		that.src = source;
		setupTexture();
	}
}

J3D.Texture.prototype.update = function() {
	if(!this.loaded || !this.isVideo) return;
	gl.bindTexture(gl.TEXTURE_2D, this.tex);
	gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, gl.RGBA, gl.UNSIGNED_BYTE, this.src);
	gl.bindTexture(gl.TEXTURE_2D, null);
}

J3D.Texture.prototype.toUniform = function(){
	this.update();
	return this.tex;
}
J3D.Cubemap = function(faces){
	var that = this;
	this.tex = gl.createTexture();

	this.facesLeft = 6;
	this.faceImages = {};

	var onLoad = function() {
		gl.bindTexture(gl.TEXTURE_CUBE_MAP, that.tex);

		gl.texImage2D(gl.TEXTURE_CUBE_MAP_POSITIVE_X, 0, gl.RGBA, gl.RGBA, gl.UNSIGNED_BYTE, that.faceImages.front);
		gl.texImage2D(gl.TEXTURE_CUBE_MAP_NEGATIVE_X, 0, gl.RGBA, gl.RGBA, gl.UNSIGNED_BYTE, that.faceImages.back);

		gl.texImage2D(gl.TEXTURE_CUBE_MAP_POSITIVE_Y, 0, gl.RGBA, gl.RGBA, gl.UNSIGNED_BYTE, that.faceImages.up);
		gl.texImage2D(gl.TEXTURE_CUBE_MAP_NEGATIVE_Y, 0, gl.RGBA, gl.RGBA, gl.UNSIGNED_BYTE, that.faceImages.down);

		gl.texImage2D(gl.TEXTURE_CUBE_MAP_POSITIVE_Z, 0, gl.RGBA, gl.RGBA, gl.UNSIGNED_BYTE, that.faceImages.right);
		gl.texImage2D(gl.TEXTURE_CUBE_MAP_NEGATIVE_Z, 0, gl.RGBA, gl.RGBA, gl.UNSIGNED_BYTE, that.faceImages.left);

		gl.texParameteri(gl.TEXTURE_CUBE_MAP, gl.TEXTURE_MAG_FILTER, gl.LINEAR);
		gl.texParameteri(gl.TEXTURE_CUBE_MAP, gl.TEXTURE_MIN_FILTER, gl.LINEAR);

		gl.texParameteri(gl.TEXTURE_CUBE_MAP, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
		gl.texParameteri(gl.TEXTURE_CUBE_MAP, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);

		gl.generateMipmap(gl.TEXTURE_CUBE_MAP);

		gl.bindTexture(gl.TEXTURE_CUBE_MAP, null);
	}

	var onFace = function() {
		that.facesLeft--;
		if(that.facesLeft == 0) onLoad();
	}

	var load = function(name, src){
/*

 		that.faceImages[name] = new Image();
    	that.faceImages[name].onload = function() {
			onFace();
    	}
		that.faceImages[name] .src = src;
*/
	  console.log ("loading image from " + ejspath.resolve (process.cwd(), src));
	  that.faceImages[name] = uikit.UIImage.imageWithContentsOfFile (ejspath.resolve (process.cwd(), src));
	  console.log ("image = " + (that.faceImages[name] || "<null>"));
	};


	load("left", 	faces.left);
	load("right", 	faces.right);
	load("up", 		faces.up);
	load("down", 	faces.down);
	load("back", 	faces.back);
	load("front", 	faces.front);
	onLoad();
}

J3D.Cubemap.prototype.toUniform = function(){
	return this.tex;
}
J3D.Transform = function(n, u){
	var that = this;
	
	this.uid = u || 0;
	this.name = n;
	
	var children = [];
	this.numChildren = 0;
	
	// All local
	this.position = v3.ZERO();
	this.rotation = v3.ZERO();
	this.scale = v3.ONE();
	
	// This gets only updated for lights
	this.worldPosition = v3.ZERO();
	
	// Local transformation matrix
	this.matrix = mat4.create();
	// World transformation matrix (concatenated local transforms of all parents and self)
	this.globalMatrix = mat4.create();
	// Normal matrix (inverse/transpose of view matrix for use with normals)
	this.normalMatrix = mat3.create();

	this.isStatic = false;
	this._lockedMatrix = false;
	this.enabled = true;
	
	this.renderer;	
	this.geometry;
	this.camera;
	this.light;
	
	// Texture tile and offset.
	// Can also be specified in the renderer, but this will override
	// the settings for this specific object unless tile = 1 and offset = 0
	this.textureTile = v2.ONE();
	this.textureOffset = v2.ZERO();

	this.add = function(t){
		children.push(t);
		that.numChildren = children.length;
		return t;
	}
	
	this.childAt = function(i){
		return children[i];
	}	
}

J3D.Transform.prototype.clone = function(){
	var c = new J3D.Transform();
	c.position = this.position.cp();
	c.rotation = this.rotation.cp();
	c.scale = this.scale.cp();
	
	c.isStatic = this.isStatic;
	
	c.renderer = this.renderer;
	c.mesh = this.mesh;
	c.camera = this.camera;
	c.light = this.light;
	
	return c;
}

J3D.Transform.prototype.forward = function() {
	// TODO: optimize
	var tm = mat4.create();
	var tv = mat4.multiplyVec3( mat3.toMat4(this.normalMatrix, tm), [0,0,1]);
	return new v3(tv[0], tv[1], tv[2]).norm();
}

J3D.Transform.prototype.left = function() {
	// TODO: optimize
	var tm = mat4.create();
	var tv = mat4.multiplyVec3( mat3.toMat4(this.normalMatrix, tm), [1,0,0]);
	return new v3(tv[0], tv[1], tv[2]).norm();
}

J3D.Transform.prototype.updateWorld = function(parent){
	if(this._lockedMatrix) return;
	
	mat4.identity(this.matrix);
	
	mat4.translate(this.matrix, [this.position.x, this.position.y, this.position.z]);

	mat4.rotateZ(this.matrix, this.rotation.z);
	mat4.rotateX(this.matrix, this.rotation.x);
	mat4.rotateY(this.matrix, this.rotation.y);

	mat4.scale(this.matrix, [this.scale.x, this.scale.y, this.scale.z]);

	if(parent != null) mat4.multiply(parent.globalMatrix, this.matrix, this.globalMatrix);
	else this.globalMatrix = this.matrix;
	
	mat4.toInverseMat3(this.globalMatrix, this.normalMatrix);
	mat3.transpose(this.normalMatrix);
	
	if(this.isStatic) this._lockedMatrix = true;
}

J3D.Transform.prototype.updateWorldPosition = function() {
	var tmp = [0,0,0];	
	mat4.multiplyVec3(this.globalMatrix, tmp);
	this.worldPosition.x = tmp[0];
	this.worldPosition.y = tmp[1];
	this.worldPosition.z = tmp[2];
}

J3D.Transform.prototype.getTileOffset = function() {
	var t, o;
	
	if(this.renderer.textureTile && this.textureTile.isOne()) t = this.renderer.textureTile.xy();
	else t = this.textureTile.xy();
	
	if(this.renderer.textureOffset && this.textureOffset.isZero()) o = this.renderer.textureOffset.xy();
	else o = this.textureOffset.xy();

	return t.concat(o);
}

J3D.Transform.prototype.find = function(p) {
	
	for(var i = 0; i < this.numChildren; i++) {
		if(this.childAt(i).name == p[0]) {
			if(p.length == 1) return this.childAt(i);
			else return this.childAt(i).find(p.slice(1));
		}
	}
	
	return null;
}

// Used if transform is a camera
J3D.Transform.prototype.updateInverseMat = function(transform) {
	if(!this.inverseMat) this.inverseMat = mat4.create();
	mat4.inverse(this.globalMatrix, this.inverseMat);
	this.updateWorldPosition();
}

J3D.Postprocess = function(engine) {
	this.drawMode = gl.TRIANGLES;
	this.engine = engine;
	this.fbo = new J3D.FrameBuffer();
	
	this.geometry = J3D.Primitive.FullScreenQuad();
	this.filter = null;
}

J3D.Postprocess.prototype.render = function() {
	this.fbo.bind();
	this.engine.render();
	this.fbo.unbind();
	this.renderEffect(this.fbo.texture);
}
	
J3D.Postprocess.prototype.renderEffect = function(texture) {
	this.program = this.engine.shaderAtlas.getShader(this.filter);
	
	this.filter.setup(this.program);
	
	gl.clear(gl.COLOR_BUFFER_BIT | gl.DEPTH_BUFFER_BIT);
	gl.useProgram(this.program);
	
	if(this.program.uniforms.uTime) gl.uniform1f(this.program.uniforms.uTime.location, J3D.Time.time);
	J3D.ShaderUtil.setTexture(this.program, 0, "uTexture", texture);

	for(var i = 0; i < this.geometry.arrays.length; i++) {
		var vbo = this.geometry.arrays[i];	
		if(this.program.attributes[vbo.name] != null) {
			gl.bindBuffer(gl.ARRAY_BUFFER, vbo.buffer);
			gl.vertexAttribPointer(this.program.attributes[vbo.name], vbo.itemSize, gl.FLOAT, false, 0, 0);
		}
	}

	gl.drawArrays(this.drawMode, 0, this.geometry.size);
}


J3D.Primitive = {};

J3D.Primitive.Cube = function(w, h, d) {
	var c = J3D.Primitive.getEmpty();
	w = w * 0.5;
	h = h * 0.5;
	d = d * 0.5;
	
	J3D.Primitive.addQuad(c, new v3(-w, h, d), new v3(w, h, d), new v3(w, -h, d), new v3(-w, -h, d));
	J3D.Primitive.addQuad(c, new v3(w, h, -d), new v3(-w, h, -d), new v3(-w, -h, -d), new v3(w, -h, -d));
	
	J3D.Primitive.addQuad(c, new v3(-w, h, -d), new v3(-w, h, d), new v3(-w, -h, d), new v3(-w, -h, -d));
	J3D.Primitive.addQuad(c, new v3(w, h, d), new v3(w, h, -d), new v3(w, -h, -d), new v3(w, -h, d));
	
	J3D.Primitive.addQuad(c, new v3(w, h, d), new v3(-w, h, d), new v3(-w, h, -d), new v3(w, h, -d));
	J3D.Primitive.addQuad(c, new v3(w, -h, d), new v3(w, -h, -d), new v3(-w, -h, -d), new v3(-w, -h, d));

	return new J3D.Mesh(c);
}

J3D.Primitive.FullScreenQuad = function() {
	var c = new J3D.Geometry();
	c.addArray("aVertexPosition", new Float32Array([-1, 1,     1, 1,     1, -1,     -1, 1,     1, -1,     -1, -1]), 2);
	c.addArray("aTextureCoord", new Float32Array([0, 1,     1, 1,     1, 0,     0, 1,     1, 0,    0, 0]), 2);
	return c;
}

J3D.Primitive.Plane = function(w, h, wd, hd, wo, ho) {
	var c = J3D.Primitive.getEmpty();
	
	if(!wo) wo = 0;
	if(!ho) ho = 0;
 	
	w = w * 0.5;
	h = h * 0.5;
	
	if(!wd) wd = 1;
	if(!hd) hd = 1;
	
	var wStart = -w + wo;
	var wEnd = w + wo;
	var hStart = h + ho;
	var hEnd = -h + ho;
	var uStart = 0;
	var uEnd = 1;
	var vStart = 1;
	var vEnd = 0;
	
	var wb = (w * 2) / wd;
	var hb = (h * 2) / hd;
	
	for(var i = 0; i < wd; i++) {
		for(var j = 0; j < hd; j++) {
			
			var bvStart = wStart + i * wb;
			var bvEnd = bvStart + wb;
			var bhStart = hStart - j * hb;
			var bhEnd = bhStart - hb;
			
			var va = new v3(bvStart, bhStart, 0);
			var vb = new v3(bvEnd, bhStart, 0);
			var vc = new v3(bvEnd, bhEnd, 0);
			var vd = new v3(bvStart, bhEnd, 0);
			
			var us = 1 / wd * i;
			var ue = 1 / wd * (i + 1);
			var vs = 1 - (1 / hd * (j + 1));
			var ve = 1 - (1 / hd * j);
			
			J3D.Primitive.addQuad(c, va, vb, vc, vd, us, ue, vs, ve);
		}
	}

	return new J3D.Mesh(c);
}

J3D.Primitive.getEmpty = function(){
	var g = {};
	g.vertices = [];	 
	g.normals = [];
	g.uv1 = [];
	g.tris = [];
	return g;
}

J3D.Primitive.addQuad = function(g, p1, p2, p3, p4, minU, maxU, minV, maxV) {
	var n1 = v3.cross(p1.sub(p2), p2.sub(p3)).norm();
	var p = g.vertices.length / 3;
	
	var nu = (minU) ? minU : 0;
	var xu = (maxU) ? maxU : 1;
	var nv = (minV) ? minV : 0;
	var xv = (maxV) ? maxV : 1;
	
		
	g.vertices.push(p1.x, p1.y, p1.z, p2.x, p2.y, p2.z, p3.x, p3.y, p3.z, p4.x, p4.y, p4.z);
	g.normals.push (n1.x, n1.y, n1.z, n1.x, n1.y, n1.z, n1.x, n1.y, n1.z, n1.x, n1.y, n1.z);
	g.uv1.push(nu,xv, xu,xv, xu,nv, nu,nv);
	
	g.tris.push(p, p + 1, p + 2, p, p + 2, p + 3);
}
J3D.FrameBuffer = function(width, height){
	
	this.width = (width) ? width : gl.viewportWidth;
	this.height = (height) ? height : gl.viewportHeight;
	
	this.fbo = gl.createFramebuffer();
	gl.bindFramebuffer(gl.FRAMEBUFFER, this.fbo);
	
	this.texture = gl.createTexture();
	gl.bindTexture(gl.TEXTURE_2D, this.texture);
	gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.LINEAR);
	gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR);
	gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
	gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
	
	gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, this.width, this.height, 0, gl.RGBA, gl.UNSIGNED_BYTE, null);
	
	this.depthBuffer = gl.createRenderbuffer();
	gl.bindRenderbuffer(gl.RENDERBUFFER, this.depthBuffer);
	gl.renderbufferStorage(gl.RENDERBUFFER, gl.DEPTH_COMPONENT16, this.width, this.height);
	
	gl.framebufferTexture2D(gl.FRAMEBUFFER, gl.COLOR_ATTACHMENT0, gl.TEXTURE_2D, this.texture, 0);
	gl.framebufferRenderbuffer(gl.FRAMEBUFFER, gl.DEPTH_ATTACHMENT, gl.RENDERBUFFER, this.depthBuffer);
	
	gl.bindTexture(gl.TEXTURE_2D, null);
	gl.bindRenderbuffer(gl.RENDERBUFFER, null);
	gl.bindFramebuffer(gl.FRAMEBUFFER, null);
}

J3D.FrameBuffer.prototype.bind = function(){
	gl.bindFramebuffer(gl.FRAMEBUFFER, this.fbo);
}

J3D.FrameBuffer.prototype.unbind = function(){
	gl.bindFramebuffer(gl.FRAMEBUFFER, null);
}

// File generated with util/buildShaders.py. Do not edit //

J3D.ShaderSource = {};

J3D.ShaderSource.Depth = "\
//#name Depth\n\
//#author bartekd\n\
\n\
//#include CommonInclude\n\
\n\
//#vertex\n\
//#include VertexInclude\n\
varying float depth;\n\
\n\
void main(void) {\n\
vec4 p = mMatrix * vec4(aVertexPosition, 1.0);\n\
gl_Position = pMatrix * vMatrix * p;\n\
depth = gl_Position.z/gl_Position.w;\n\
}\n\
\n\
//#fragment\n\
varying float depth;\n\
\n\
void main(void) {\n\
float d = 1.0 - depth;\n\
\n\
gl_FragColor = vec4(d, d, d, 1.0);\n\
}\n\
";

J3D.ShaderSource.Gouraud = "\
//#name Gouraud\n\
//#author bartekd\n\
\n\
//#include CommonInclude\n\
\n\
//#vertex\n\
//#include VertexInclude\n\
uniform float specularIntensity;\n\
uniform float shininess;\n\
\n\
varying vec3 vLight;\n\
varying vec2 vTextureCoord;\n\
\n\
void main(void) {\n\
vec4 p = mMatrix * vec4(aVertexPosition, 1.0);\n\
gl_Position = pMatrix * vMatrix * p;\n\
vTextureCoord = getTextureCoord(aTextureCoord);\n\
vec3 n = normalize( nMatrix * aVertexNormal );\n\
vLight = computeLights(p, n, specularIntensity, shininess);\n\
}\n\
\n\
//#fragment\n\
uniform vec4 color;\n\
uniform sampler2D colorTexture;\n\
uniform bool hasColorTexture;\n\
\n\
varying vec3 vLight;\n\
varying vec2 vTextureCoord;\n\
\n\
void main(void) {\n\
vec4 tc = color;\n\
if(hasColorTexture) tc *= texture2D(colorTexture, vTextureCoord);\n\
gl_FragColor = vec4(tc.rgb * vLight, color.a);\n\
}\n\
";

J3D.ShaderSource.Lightmap = "\
//#name Lightmap\n\
//#author bartekd\n\
\n\
//#include CommonInclude\n\
\n\
//#vertex\n\
//#include VertexInclude\n\
uniform vec4 lightmapAtlas;\n\
\n\
varying vec2 vTextureCoord;\n\
varying vec2 vTextureCoord2;\n\
\n\
void main(void) {\n\
vTextureCoord = getTextureCoord(aTextureCoord);\n\
vTextureCoord2 = aTextureCoord2 * lightmapAtlas.xy + lightmapAtlas.zw;\n\
\n\
gl_Position = mvpMatrix() * vec4(aVertexPosition, 1.0);\n\
}\n\
\n\
//#fragment\n\
uniform vec4 color;\n\
uniform sampler2D colorTexture;\n\
uniform sampler2D lightmapTexture;\n\
\n\
varying vec2 vTextureCoord;\n\
varying vec2 vTextureCoord2;\n\
\n\
void main(void) {\n\
\n\
vec4 tc = texture2D(colorTexture, vTextureCoord);\n\
vec4 lm = texture2D(lightmapTexture, vTextureCoord2);\n\
\n\
if(tc.a < 0.1) discard;\n\
else gl_FragColor = vec4(color.rgb * tc.rgb * lm.rgb, 1.0);\n\
}\n\
";

J3D.ShaderSource.Normal2Color = "\
//#name Normal2Color\n\
//#author bartekd\n\
\n\
//#include CommonInclude\n\
\n\
//#vertex\n\
//#include VertexInclude\n\
varying vec3 vColor;\n\
\n\
void main(void) {\n\
gl_Position = mvpMatrix() * vec4(aVertexPosition, 1.0);\n\
vColor = normalize( aVertexNormal / 2.0 + vec3(0.5) );\n\
}\n\
\n\
//#fragment\n\
varying vec3 vColor;\n\
\n\
void main(void) {\n\
gl_FragColor = vec4(vColor, 1.0);\n\
}\n\
";

J3D.ShaderSource.Phong = "\
//#name Phong\n\
//#description Classic phong shader\n\
//#author bartekd\n\
\n\
//#include CommonInclude\n\
\n\
//#vertex\n\
//#include VertexInclude\n\
varying vec4 vPosition;\n\
varying vec3 vLight;\n\
varying vec2 vTextureCoord;\n\
varying vec3 vNormal;\n\
\n\
void main(void) {\n\
vTextureCoord = getTextureCoord(aTextureCoord);\n\
vNormal = nMatrix * aVertexNormal;\n\
vPosition = mMatrix * vec4(aVertexPosition, 1.0);\n\
gl_Position = pMatrix * vMatrix * vPosition;\n\
gl_PointSize = 5.0;\n\
}\n\
\n\
//#fragment\n\
uniform vec4 color;\n\
uniform sampler2D colorTexture;\n\
uniform bool hasColorTexture;\n\
uniform float specularIntensity;\n\
uniform float shininess;\n\
\n\
varying vec4 vPosition;\n\
varying vec3 vLight;\n\
varying vec2 vTextureCoord;\n\
varying vec3 vNormal;\n\
\n\
void main(void) {\n\
vec4 tc = color;\n\
if(hasColorTexture) tc *= texture2D(colorTexture, vTextureCoord);\n\
vec3 l = computeLights(vPosition, vNormal, specularIntensity, shininess);\n\
gl_FragColor = vec4(tc.rgb * l, color.a);\n\
}\n\
";

J3D.ShaderSource.Reflective = "\
//#name Reflective\n\
//#description Based on Cg tutorial: http://http.developer.nvidia.com/CgTutorial/cg_tutorial_chapter07.html\n\
//#author bartekd\n\
\n\
//#include CommonInclude\n\
\n\
//#vertex\n\
//#include VertexInclude\n\
\n\
varying vec3 vNormal;\n\
varying vec3 refVec;\n\
\n\
void main(void) {\n\
gl_Position = mvpMatrix() * vec4(aVertexPosition, 1.0);\n\
vNormal = normalize(nMatrix * aVertexNormal);\n\
vec3 incident = normalize( (vec4(aVertexPosition, 1.0) * mMatrix).xyz - uEyePosition);\n\
refVec = reflect(incident, vNormal);\n\
}\n\
\n\
//#fragment\n\
uniform samplerCube uCubemap;\n\
\n\
varying vec3 refVec;\n\
\n\
void main(void) {\n\
gl_FragColor = textureCube(uCubemap, refVec);\n\
}\n\
";

J3D.ShaderSource.Skybox = "\
//#name Skybox\n\
//#author bartekd\n\
\n\
//#include CommonInclude\n\
\n\
//#vertex\n\
//#include VertexInclude\n\
uniform float mid;\n\
\n\
varying vec3 vVertexPosition;\n\
\n\
void main(void) {\n\
gl_Position = pMatrix * vMatrix * vec4(uEyePosition + aVertexPosition * mid, 1.0);\n\
vVertexPosition = aVertexPosition;\n\
}\n\
\n\
//#fragment\n\
uniform samplerCube uCubemap;\n\
\n\
varying vec3 vVertexPosition;\n\
\n\
void main(void) {\n\
gl_FragColor = textureCube(uCubemap, vVertexPosition);\n\
}\n\
";

J3D.ShaderSource.Toon = "\
//#name Toon\n\
//#author bartekd\n\
\n\
//#include CommonInclude\n\
\n\
//#vertex\n\
//#include VertexInclude\n\
varying float vLight;\n\
varying vec2 vTextureCoord;\n\
\n\
float cli(vec4 p, vec3 n, lightSource light){\n\
vec3 ld;\n\
if(light.type == 0) return 0.0;\n\
else if(light.type == 1) ld = -light.direction;\n\
else if(light.type == 2) ld = normalize(light.position - p.xyz);\n\
return max(dot(n, ld), 0.0);\n\
}\n\
\n\
float lightIntensity(vec4 p, vec3 n) {\n\
float s = cli(p, n, uLight[0]);\n\
s += cli(p, n, uLight[1]);\n\
s += cli(p, n, uLight[2]);\n\
s += cli(p, n, uLight[3]);\n\
return s;\n\
}\n\
\n\
void main(void) {\n\
vec4 p = mMatrix * vec4(aVertexPosition, 1.0);\n\
gl_Position = pMatrix * vMatrix * p;\n\
gl_PointSize = 10.0;\n\
vTextureCoord = getTextureCoord(aTextureCoord);\n\
vec3 n = normalize( nMatrix * aVertexNormal );\n\
vLight = lightIntensity(p, n);\n\
}\n\
\n\
//#fragment\n\
uniform vec4 uColor;\n\
uniform sampler2D uColorSampler;\n\
\n\
varying float vLight;\n\
varying vec2 vTextureCoord;\n\
\n\
void main(void) {\n\
vec4 tc = texture2D(uColorSampler, vec2(vLight, 0.5) );\n\
gl_FragColor = vec4(tc.rgb, 1.0);\n\
}\n\
";

J3D.ShaderSource.Vignette = "\
//#name Vignette\n\
//#author bartekd\n\
\n\
//#vertex\n\
//#include BasicFilterVertex\n\
\n\
//#fragment\n\
//#include CommonFilterInclude\n\
uniform sampler2D uTexture;\n\
varying vec2 vTextureCoord;\n\
\n\
void main(void) {\n\
vec2 m = vec2(0.5, 0.5);\n\
float d = distance(m, vTextureCoord) * 1.0;\n\
vec3 c = texture2D(uTexture, vTextureCoord).rgb * (1.0 - d * d);\n\
gl_FragColor = vec4(c.rgb, 1.0);\n\
}\n\
";

J3D.ShaderSource.BasicFilterVertex = "\
//#name BasicFilterVertex\n\
//#description A basic vertex shader for filters that use a full screen quad and have all the logic in fragment shader\n\
attribute vec2 aVertexPosition;\n\
attribute vec2 aTextureCoord;\n\
\n\
varying vec2 vTextureCoord;\n\
\n\
void main(void) {\n\
gl_Position = vec4(aVertexPosition, 0.0, 1.0);\n\
vTextureCoord = aTextureCoord;\n\
}\n\
\n\
";

J3D.ShaderSource.CommonFilterInclude = "\
//#name CommonFilterInclude\n\
//#description Common uniforms and function for filters\n\
#ifdef GL_ES\n\
precision highp float;\n\
#endif\n\
\n\
uniform float uTime;\n\
\n\
float whiteNoise(vec2 uv, float scale) {\n\
float x = (uv.x + 0.2) * (uv.y + 0.2) * (10000.0 + uTime);\n\
x = mod( x, 13.0 ) * mod( x, 123.0 );\n\
float dx = mod( x, 0.005 );\n\
return clamp( 0.1 + dx * 100.0, 0.0, 1.0 ) * scale;\n\
}\n\
";

J3D.ShaderSource.CommonInclude = "\
//#name CommonInclude\n\
//#description Collection of common uniforms, functions and structs to include in shaders (both fragment and vertex)\n\
#ifdef GL_ES\n\
precision highp float;\n\
#endif\n\
\n\
struct lightSource {\n\
int type;\n\
vec3 direction;\n\
vec3 color;\n\
vec3 position;\n\
};\n\
\n\
uniform float uTime;\n\
uniform mat4 mMatrix;\n\
uniform mat4 vMatrix;\n\
uniform mat3 nMatrix;\n\
uniform mat4 pMatrix;\n\
uniform vec3 uEyePosition;\n\
uniform lightSource uLight[4];\n\
uniform vec3 uAmbientColor;\n\
uniform vec4 uTileOffset;\n\
\n\
mat4 mvpMatrix() {\n\
return pMatrix * vMatrix * mMatrix;\n\
}\n\
\n\
mat4 mvMatrix() {\n\
return vMatrix * mMatrix;\n\
}\n\
\n\
float luminance(vec3 c) {\n\
return c.r * 0.299 + c.g * 0.587 + c.b * 0.114;\n\
}\n\
\n\
float brightness(vec3 c) {\n\
return c.r * 0.2126 + c.g * 0.7152 + c.b * 0.0722;\n\
}\n\
\n\
vec3 computeLight(vec4 p, vec3 n, float si, float sh, lightSource light){\n\
vec3 ld;\n\
\n\
if(light.type == 0) return vec3(0);\n\
else if(light.type == 1) ld = -light.direction;\n\
else if(light.type == 2) ld = normalize(light.position - p.xyz);\n\
\n\
float dif = max(dot(n, ld), 0.0);\n\
\n\
float spec = 0.0;\n\
\n\
if(si > 0.0) {\n\
vec3 eyed = normalize(uEyePosition - p.xyz);\n\
vec3 refd = reflect(-ld, n);\n\
spec = pow(max(dot(refd, eyed), 0.0), sh) * si;\n\
};\n\
\n\
return light.color * dif + light.color * spec;\n\
}\n\
\n\
vec3 computeLights(vec4 p, vec3 n, float si, float sh) {\n\
vec3 s = uAmbientColor;\n\
s += computeLight(p, n, si, sh, uLight[0]);\n\
s += computeLight(p, n, si, sh, uLight[1]);\n\
s += computeLight(p, n, si, sh, uLight[2]);\n\
s += computeLight(p, n, si, sh, uLight[3]);\n\
return s;\n\
}\n\
\n\
vec2 getTextureCoord(vec2 uv) {\n\
return uv * uTileOffset.xy + uTileOffset.zw;\n\
}\n\
";

J3D.ShaderSource.Modifiers = "\
//#name Modifiers\n\
//#description A collection of modifier functions for geometry (only bend for now)\n\
\n\
vec3 bend(vec3 ip, float ba, vec2 b, float o, float a) {\n\
vec3 op = ip;\n\
\n\
ip.x = op.x * cos(a) - op.y * sin(a);\n\
ip.y = op.x * sin(a) + op.y * cos(a);\n\
\n\
if(ba != 0.0) {\n\
float radius = b.y / ba;\n\
float onp = (ip.x - b.x) / b.y - o;\n\
ip.z = cos(onp * ba) * radius - radius;\n\
ip.x = (b.x + b.y * o) + sin(onp * ba) * radius;\n\
}\n\
\n\
op = ip;\n\
ip.x = op.x * cos(-a) - op.y * sin(-a);\n\
ip.y = op.x * sin(-a) + op.y * cos(-a);\n\
\n\
return ip;\n\
}\n\
";

J3D.ShaderSource.VertexInclude = "\
//#name VertexInclude\n\
//#description Common attributes for a mesh - include this in a vertex shader so you don't rewrite this over and over again\n\
attribute vec3 aVertexPosition;\n\
attribute vec3 aVertexNormal;\n\
attribute vec2 aTextureCoord;\n\
attribute vec2 aTextureCoord2;\n\
attribute vec4 aVertexColor;\n\
";
J3D.Shader = function(n, v, f, m) {
	if(!n) throw new Error("You must specify a name for custom shaders");
	if(v == null || f == null) throw new Error("You must pass a vertex and fragment shader source for custom shaders");

	this.name = n;
	this.drawMode = 0x0004;// <- gl.TRIANGLES, but since it can be called before J3D.Engine and gl are initialized, let's use the value directly

	this._vertSource = v;
	this._fragSource = f;

	this.reloadStaticUniforms = true;
	this.su = {};
	this.loadedStaticTextures = {};

	this.metaData = m || {};
}

J3D.Shader.prototype.vertSource = function() {
	return this._vertSource;
}

J3D.Shader.prototype.fragSource = function() {
	return this._fragSource;
}

J3D.Shader.prototype.setup = function(shader, transform) {
	if(this.reloadStaticUniforms) {
		this.loadedStaticTextures = {};
	}

	var t = 0;
	for(var s in shader.uniforms) {
		if (this.reloadStaticUniforms && this.su[s] != null && this[s] == null && this.su[s].loaded == null) {
			J3D.ShaderUtil.setUniform(s, shader, this.su);
		}

		if(this.su[s] != null && this[s] == null && this.su[s].loaded && !this.loadedStaticTextures[s]) {
			J3D.ShaderUtil.setUniform(s, shader, this.su);
			this.loadedStaticTextures[s] = true;
		}

		if (this[s] != null) {
			t++;
			J3D.ShaderUtil.setUniform(s, shader, this);
		}
	}
	this.reloadStaticUniforms = false;
	j3dlogOnce("Shader " + this.name + " has " + t + " dynamic uniforms");
}

J3D.Shader.prototype.clone = function() {
	var c = new J3D.Shader(this.name + Math.random(), this._vertSource, this._fragSource);

	for(let s in this) {
		if (typeof this[s] !== "function" && this.hasOwnProperty(s)) {
			c[s] = this[s];
		}
	}

	if (this.hasOwnProperty("setup")) {
		c.setup = this.setup;
	}

	c.su = {};

	for(let ss in this.su) {
		if (typeof this.su[ss] !== "function" && this.su.hasOwnProperty(ss)) {
			c.su[ss] = this.su[ss];
		}
	}

	c.reloadStaticUniforms = true;

	return c;
}
J3D.Time = {}

J3D.Time.time = 0;
J3D.Time.startTime = 0;
J3D.Time.lastTime = 0;
J3D.Time.deltaTime = 0;

J3D.Time.tick = function() {
	var tn = new Date().getTime();
	
	if (J3D.Time.startTime == 0) J3D.Time.startTime = tn;
    if (J3D.Time.lastTime != 0) J3D.Time.deltaTime = tn - J3D.Time.lastTime;
	
    J3D.Time.lastTime = tn;
	J3D.Time.time = (tn - J3D.Time.startTime) / 1000.0;
};

J3D.Time.formatTime = function() {
	var mil = Math.floor((J3D.Time.time % 1) * 100);
	var sec = Math.floor(J3D.Time.time) % 60;
	var min = Math.floor(J3D.Time.time / 60);
	
	if(mil < 10) mil = "0" + mil;
	if(mil == 100) mil = "00";
	
	if(sec < 10) sec = "0" + sec;
	if(min < 10) min = "0" + min;

	return min + ":" + sec + ":" + mil;
}
J3D.ShaderUtil = {};

J3D.ShaderUtil.setTexture = function(shader, id, uniformName, texture){
	gl.activeTexture(33984 + id);
	gl.bindTexture(gl.TEXTURE_2D, texture);
	gl.uniform1i(shader.uniforms[uniformName].location, id);
}

J3D.ShaderUtil.setTextureCube = function(shader, id, uniformName, texture){
	gl.activeTexture(33984 + id);
	gl.bindTexture(gl.TEXTURE_CUBE_MAP, texture);
	gl.uniform1i(shader.uniforms[uniformName].location, id);
}

J3D.ShaderUtil.setLights = function(shader, lights) {
	for (var i = 0; i < J3D.SHADER_MAX_LIGHTS; i++) {
		var l = lights[i];
		if(l && shader.uniforms["uLight[" + i + "].type"]){
			gl.uniform1i(shader.uniforms["uLight[" + i + "].type"].location, 		lights[i].light.type);
			gl.uniform3fv(shader.uniforms["uLight[" + i + "].direction"].location, 	lights[i].light.direction.xyz());
			gl.uniform3fv(shader.uniforms["uLight[" + i + "].color"].location, 		lights[i].light.color.rgb());
			gl.uniform3fv(shader.uniforms["uLight[" + i + "].position"].location, 	lights[i].worldPosition.xyz());			
		} else if(shader.uniforms["uLight[" + i + "].type"]) {
			gl.uniform1i(shader.uniforms["uLight[" + i + "].type"].location, J3D.NONE);
		} else {
			//console.log("Light not set " + i);
		}
	}
}

J3D.ShaderUtil.isTexture = function(t) {
	return t == gl.SAMPLER_2D || t == gl.SAMPLER_CUBE;
}

J3D.ShaderUtil.getTypeName = function(t) {
	switch(t) {
		case gl.BYTE:   	  return "BYTE (0x1400)";
		case gl.UNSIGNED_BYTE:return "UNSIGNED_BYTE (0x1401)";
		case gl.SHORT:   	  return "SHORT (0x1402)";
		case gl.UNSIGNED_SHORT:return "UNSIGNED_SHORT (0x1403)";
		case gl.INT:   		  return "INT (0x1404)";
		case gl.UNSIGNED_INT: return "UNSIGNED_INT (0x1405)";
		case gl.FLOAT:   	  return "FLOAT (0x1406)";
		case gl.FLOAT_VEC2:   return "FLOAT_VEC2 (0x8B50)";
		case gl.FLOAT_VEC3:   return "FLOAT_VEC3 (0x8B51)";
		case gl.FLOAT_VEC4:   return "FLOAT_VEC4 (0x8B52)";
		case gl.INT_VEC2:     return "INT_VEC2   (0x8B53)";
		case gl.INT_VEC3:     return "INT_VEC3   (0x8B54)";
		case gl.INT_VEC4:     return "INT_VEC4   (0x8B55)";
		case gl.BOOL:         return "BOOL 		(0x8B56)";
		case gl.BOOL_VEC2:    return "BOOL_VEC2  (0x8B57)";
		case gl.BOOL_VEC3:    return "BOOL_VEC3  (0x8B58)";
		case gl.BOOL_VEC4:    return "BOOL_VEC4  (0x8B59)";
		case gl.FLOAT_MAT2:   return "FLOAT_MAT2 (0x8B5A)";
		case gl.FLOAT_MAT3:   return "FLOAT_MAT3 (0x8B5B)";
		case gl.FLOAT_MAT4:   return "FLOAT_MAT4 (0x8B5C)";
		case gl.SAMPLER_2D:   return "SAMPLER_2D (0x8B5E)";
		case gl.SAMPLER_CUBE: return "SAMPLER_CUBE (0x8B60)";
		default: return "Unknown (" + t.toString(16) + ")";
	}
}

J3D.ShaderUtil.setUniform = function(name, dst, src) {
	var n = dst.uniforms[name];
	if(!n) return;

	var v = src[name];
	if(v.toUniform) v = v.toUniform(n.type);

	switch (n.type) {
		case gl.BYTE:
			gl.uniform1i(n.location, v);
			break;
		case gl.UNSIGNED_BYTE:
			gl.uniform1i(n.location, v);
			break;
		case gl.SHORT:
			gl.uniform1i(n.location, v);
			break;
		case gl.UNSIGNED_SHORT:
			gl.uniform1i(n.location, v);
			break;
		case gl.INT:
			gl.uniform1i(n.location, v);
			break;
		case gl.INT_VEC2:
			gl.uniform2iv(n.location, v);
			break;
		case gl.INT_VEC3:
			gl.uniform3iv(n.location, v);
			break;
		case gl.INT_VEC4:
			gl.uniform4iv(n.location, v);
			break;
		case gl.UNSIGNED_INT:
			gl.uniform1i(n.location, v);
			break;
		case gl.FLOAT:
			gl.uniform1f(n.location, v);
			break;
		case gl.FLOAT_VEC2:
			gl.uniform2fv(n.location, v);
			break;
		case gl.FLOAT_VEC3:
			gl.uniform3fv(n.location, v);
			break;
		case gl.FLOAT_VEC4:
			gl.uniform4fv(n.location, v);
			break;
		case gl.BOOL:
			gl.uniform1i(n.location, v);
			break;
		case gl.BOOL_VEC2:
			gl.uniform2iv(n.location, v);
			break;
		case gl.BOOL_VEC3:
			gl.uniform3iv(n.location, v);
			break;
		case gl.BOOL_VEC4:
			gl.uniform4iv(n.location, v);
			break;
		// TODO: Test matrices
		case gl.FLOAT_MAT2:
			gl.uniformMatrix2fv(n.location, false, v);
			break;
		case gl.FLOAT_MAT3:
			gl.uniformMatrix3fv(n.location, false, v);
			break;
		case gl.FLOAT_MAT4:
			gl.uniformMatrix4fv(n.location, false, v);
			break;
		case gl.SAMPLER_2D:
			J3D.ShaderUtil.setTexture(dst, n.texid, name, v);
			break;
		case gl.SAMPLER_CUBE:
			J3D.ShaderUtil.setTextureCube(dst, n.texid, name, v);
			break;
		default:
			return "WARNING! Unknown uniform type ( 0x" + n.type.toString(16) + " )";
	}
}

J3D.ShaderUtil.parseGLSL = function(source){
	var ls = source.split("\n");
	
	var vs = "";
	var fs = "";

	var meta = {};
	meta.common = "";
	meta.includes = [];
	meta.vertexIncludes = [];
	meta.fragmentIncludes = [];
	var section = 0;
	
	var checkMetaData = function(tag, line) {
		var p = line.indexOf(tag);
		
		if(p > -1) {
			var d = line.substring(p + tag.length + 1);
//			j3dlog("Tag: " + tag + " (" + section + ") Value: " + d);
			return d;
		}
		
		return null;
	}
	
	for(var i = 0; i < ls.length; i++) {
		if(ls[i].indexOf("//#") > -1) {
			if (ls[i].indexOf("//#fragment") > -1) {
				section++;
			} else if (ls[i].indexOf("//#vertex") > -1) {
				section++;
			} else {	
				meta.name = meta.name || checkMetaData("name", ls[i]);
//				meta.author = meta.author || checkMetaData("author", ls[i]);
//				meta.description = meta.description || checkMetaData("description", ls[i]);
				
				var inc = checkMetaData("include", ls[i]);
				if(inc) {
					switch(section){
						case 0:
							meta.includes.push(inc); 
							break;
						case 1:
							meta.vertexIncludes.push(inc); 
							break;
						case 2:
							meta.fragmentIncludes.push(inc); 
							break;
					}
				}
			}
		} else {
			var l = ls[i];
			if(l.indexOf("//") > -1) l = l.substring(0, l.indexOf("//"));
			switch(section){
				case 0:
					meta.common += l + "\n";
					break;
				case 1:
					vs += l + "\n";
					break;
				case 2:
					fs += l + "\n";
					break;
			}
		}
	}
	
	var n = meta.name || "Shader" + Math.round(Math.random() * 1000);
	return new J3D.Shader(n, vs, fs, meta);
}
let j3dlogIds = {};

function j3dlog(m){
	if(J3D.debug) console.log(m);
}

function j3dlogOnce(m){
	if(J3D.debug && j3dlogIds[m] == null) console.log(m);
	j3dlogIds[m] = true;
}
J3D.BuiltinShaders = (function() {
	
	var shaders = {};

	var fetch = function(n) {
		if (!shaders[n]) {
			j3dlog("ERROR. Built-in shader " + n + " doesn't exist");
			return null;
		} else {
			return shaders[n].clone();
		}
	}
	
	var p = J3D.ShaderUtil.parseGLSL(J3D.ShaderSource.Phong);
	p.su.color = J3D.Color.white;
    //p.su.specularIntensity = 0;
    //p.su.shininess = 0;
	p.hasColorTexture = false;
	shaders.Phong = p;
	
	var g = J3D.ShaderUtil.parseGLSL(J3D.ShaderSource.Gouraud);
	g.su.color = J3D.Color.white;
	//g.su.specularIntensity = 0;
	//g.su.shininess = 0;
	g.hasColorTexture = false;
	shaders.Gouraud = g;
	
	var l =  J3D.ShaderUtil.parseGLSL(J3D.ShaderSource.Lightmap);
	l.setup = function(shader, transform) {
	    for (var s in shader.uniforms) {
			if (s == "lightmapTexture") {
				J3D.ShaderUtil.setTexture(shader, 1, "lightmapTexture", J3D.LightmapAtlas[transform.lightmapIndex].tex);
			} else if(s == "lightmapAtlas") {
				gl.uniform4fv(shader.uniforms.lightmapAtlas.location, transform.lightmapTileOffset);
			}
	    }
		
		J3D.Shader.prototype.setup.call(this, shader, transform);
	}
	shaders.Lightmap = l;
	
	shaders.Toon =  J3D.ShaderUtil.parseGLSL(J3D.ShaderSource.Toon);
	shaders.Reflective =  J3D.ShaderUtil.parseGLSL(J3D.ShaderSource.Reflective);
	shaders.Skybox =  J3D.ShaderUtil.parseGLSL(J3D.ShaderSource.Skybox);
	
	shaders.Normal2Color = J3D.ShaderUtil.parseGLSL(J3D.ShaderSource.Normal2Color);

	return { shaders:shaders, fetch:fetch };
}());
