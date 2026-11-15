'use strict';
// Usual reading sizes mirror src/ReaderFontSizes.h; other installed sizes stay available.
window.readerSizePolicy = Object.freeze({
  usualSizes(available) {
    if (!available.length) return [];
    return [...new Set([12,14,16,18].map(target => available.reduce((best,size) =>
      Math.abs(size-target)<Math.abs(best-target) || Math.abs(size-target)===Math.abs(best-target) && size<best ? size : best)))].sort((a,b)=>a-b);
  },
  formatSizes(points) {
    if (points.length>4 && points.every((size,index)=>index===0 || size===points[index-1]+1)) return `${points[0]}–${points[points.length-1]}`;
    return points.join(', ');
  }
});
