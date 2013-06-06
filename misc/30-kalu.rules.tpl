polkit.addRule(function(action, subject) {
   if ((action.id == "org.jjk.kalu.sysupgrade") && subject.isInGroup("@GROUP@")) {
       return polkit.Result.YES;
   }
});
