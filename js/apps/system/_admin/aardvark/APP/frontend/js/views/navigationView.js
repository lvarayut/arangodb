/*jshint browser: true */
/*jshint unused: false */
/*global Backbone, templateEngine, $, window, arangoHelper*/
(function () {
  "use strict";
  window.NavigationView = Backbone.View.extend({
    el: '#navigationBar',

    events: {
      "change #arangoCollectionSelect": "navigateBySelect",
      "click .tab": "navigateByTab",
      "click li": "switchTab",
      "click .arangodbLogo": "selectMenuItem",
      "mouseenter .dropdown > *": "showDropdown",
      "mouseleave .dropdown": "hideDropdown"
    },

    renderFirst: true,

    initialize: function () {

      this.userCollection = this.options.userCollection;
      this.currentDB = this.options.currentDB;
      this.dbSelectionView = new window.DBSelectionView({
        collection: this.options.database,
        current: this.currentDB
      });
      this.userBarView = new window.UserBarView({
        userCollection: this.userCollection
      });
      this.notificationView = new window.NotificationView({
        collection: this.options.notificationCollection
      });
      this.statisticBarView = new window.StatisticBarView({
          currentDB: this.currentDB
      });
      this.handleKeyboardHotkeys();
    },

    handleSelectDatabase: function () {
      this.dbSelectionView.render($("#dbSelect"));
    },

    template: templateEngine.createTemplate("navigationView.ejs"),

    render: function () {
      var self = this;

      $(this.el).html(this.template.render({
        currentDB: this.currentDB
      }));
      this.dbSelectionView.render($("#dbSelect"));
      this.notificationView.render($("#notificationBar"));

      var callback = function(error) {
        if (!error) {
          this.userBarView.render();
        }
      }.bind(this);

      this.userCollection.whoAmI(callback);
      this.statisticBarView.render($("#statisticBar"));

      // if demo content not available, do not show demo menu tab
      if (!window.App.arangoCollectionsStore.findWhere({"name": "arangodbflightsdemo"})) {
        $('.demo-menu').css("display","none");
      }
      if (this.renderFirst) {
        this.renderFirst = false;
          
        var select = ((window.location.hash).substr(1, (window.location.hash).length) + '-menu');
        if (select.indexOf('/') === -1) {
          this.selectMenuItem(select);
        }

        $('.arangodbLogo').on('click', function() {
          self.selectMenuItem();
        });
      }

      return this;
    },

    navigateBySelect: function () {
      var navigateTo = $("#arangoCollectionSelect").find("option:selected").val();
      window.App.navigate(navigateTo, {trigger: true});
    },

    handleKeyboardHotkeys: function () {
      arangoHelper.enableKeyboardHotkeys(true);
    },

    navigateByTab: function (e) {
      var tab = e.target || e.srcElement,
      navigateTo = tab.id,
      dropdown = false;

      if (navigateTo === "") {
        navigateTo = $(tab).attr("class");
      }
      
      if (navigateTo === "links") {
        dropdown = true;
        $("#link_dropdown").slideToggle(1);
        e.preventDefault();
      }
      else if (navigateTo === "tools") {
        dropdown = true;
        $("#tools_dropdown").slideToggle(1);
        e.preventDefault();
      }
      else if (navigateTo === "dbselection") {
        dropdown = true;
        $("#dbs_dropdown").slideToggle(1);
        e.preventDefault();
      }

      if (!dropdown) {
        window.App.navigate(navigateTo, {trigger: true});
        e.preventDefault();
      }
    },

    handleSelectNavigation: function () {
      var self = this;
      $("#arangoCollectionSelect").change(function() {
        self.navigateBySelect();
      });
    },

    switchTab: function(e) {
      var id = $(e.currentTarget).children().first().attr('id');

      if (id) {
        this.selectMenuItem(id + '-menu');
      }
    },

    selectMenuItem: function (menuItem) {
      $('.navlist li').removeClass('active');
      if (typeof menuItem === 'string') {
        if (menuItem) {
          $('.' + menuItem).addClass('active');
        }
      }
      arangoHelper.hideArangoNotifications();
    },

    showDropdown: function (e) {
      var tab = e.target || e.srcElement;
      var navigateTo = tab.id;
      if (navigateTo === "links" || navigateTo === "link_dropdown" || e.currentTarget.id === 'links') {
        $("#link_dropdown").fadeIn(1);
      }
      else if (navigateTo === "tools" || navigateTo === "tools_dropdown" || e.currentTarget.id === 'tools') {
        $("#tools_dropdown").fadeIn(1);
      }
      else if (navigateTo === "dbselection" || navigateTo === "dbs_dropdown" || e.currentTarget.id === 'dbselection') {
        $("#dbs_dropdown").fadeIn(1);
      }
    },

    hideDropdown: function (e) {
      var tab = e.target || e.srcElement;
      tab = $(tab).parent();
      $("#link_dropdown").fadeOut(1);
      $("#tools_dropdown").fadeOut(1);
      $("#dbs_dropdown").fadeOut(1);
    }

  });
}());
