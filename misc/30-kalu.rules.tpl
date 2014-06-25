polkit.addRule(function(action, subject) {
    if ((action.id == "org.jjk.kalu.sysupgrade"
            || action.id == "org.jjk.kalu.sysupgrade.downloadonly")
        && subject.isInGroup("@GROUP@")) {
       return polkit.Result.YES;
   }
});
